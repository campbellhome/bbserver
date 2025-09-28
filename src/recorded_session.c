// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "recorded_session.h"
#include "fonts.h"
#include "imgui_core.h"
#include "message_box.h"
#include "message_queue.h"
#include "path_utils.h"
#include "recorded_session_thread.h"
#include "recordings.h"
#include "span.h"
#include "tokenize.h"
#include "va.h"
#include "view.h"
#include "view_config.h"

#include "bb.h"
#include "bb_array.h"
#include "bb_assert.h"
#include "bb_malloc.h"
#include "bb_packet.h"
#include "bb_string.h"
#include "bb_thread.h"
#include "bb_time.h"

#include "bb_wrap_stdio.h"
#include <stdlib.h>

static void recorded_session_add_category(recorded_session_t* session, bb_decoded_packet_t* decoded);
static void recorded_session_add_partial_log(recorded_session_t* session, bb_decoded_packet_t* decoded, recorded_thread_t* t);
static void recorded_session_add_log(recorded_session_t* session, bb_decoded_packet_t* decoded, recorded_thread_t* t);
static void recorded_session_add_fileid(recorded_session_t* session, bb_decoded_packet_t* decoded);
static recorded_thread_t* recorded_session_find_or_add_thread(recorded_session_t* session, bb_decoded_packet_t* decoded);
static recorded_pieInstance_t* recorded_session_find_or_add_pieInstance(recorded_session_t* session, s32 pieInstance);

static sb_t s_reconstructedLogText;

typedef struct recorded_sessions_s
{
	u32 count;
	u32 allocated;
	recorded_session_t** data; // can't allocate recorded_session_t contiguously because other threads need a stable address
} recorded_sessions_t;
static recorded_sessions_t s_sessions;

static void recorded_logs_reset(recorded_logs_t* logs)
{
	for (u32 i = 0; i < logs->count; ++i)
	{
		recorded_log_t *recordedLog = logs->data[i];
		bba_free(recordedLog->lines);
		bb_free(recordedLog);
	}
	bba_free(*logs);
}

void recorded_session_shutdown(void)
{
	recorded_session_t* session;
	while ((session = recorded_session_get(0)) != NULL)
	{
		while (session->views.count)
		{
			u32 viewIndex = session->views.count - 1;
			view_t* view = session->views.data + viewIndex;
			view_reset(view);
			bba_erase(session->views, viewIndex);
		}
		recorded_session_close(session);
	}
	sb_reset(&s_reconstructedLogText);
}

void recorded_session_open(const char* path, const char* applicationFilename, const char* applicationName, b8 autoClose, b32 recordingActive, u32 outgoingMqId)
{
	if (g_config.autoCloseAll)
	{
		recorded_session_auto_close_all();
	}
	else
	{
		recorded_session_auto_close(applicationName);
	}

	view_t* view;
	recorded_session_t* session = recorded_session_find(path);
	if (!session)
	{
		recorded_session_t** holder = bba_add(s_sessions, 1);
		if (!holder)
			return;
		*holder = session = (recorded_session_t*)bb_malloc(sizeof(recorded_session_t));
		if (!session)
		{
			--s_sessions.count;
			return;
		}
		memset(session, 0, sizeof(*session));
		session->incoming = _aligned_malloc(sizeof(session_message_queue_t), 8);
		if (!session->incoming)
		{
			bb_free(session);
			--s_sessions.count;
			return;
		}
		memset(session->incoming, 0, sizeof(*session->incoming));
		bb_strncpy(session->path, path, sizeof(session->path));
		bb_strncpy(session->applicationFilename, applicationFilename, sizeof(session->applicationFilename));
		bb_critical_section_init(&session->incoming->cs);
		session->appInfo.packet.appInfo.millisPerTick = 1.0;
		session->recordingActive = (b8)recordingActive;
		if (outgoingMqId == mq_invalid_id())
		{
			session->outgoingMqId = mq_invalid_id();
		}
		else
		{
			session->outgoingMqId = mq_addref(outgoingMqId);
		}
	}

	view = bba_add(session->views, 1);
	if (!view)
	{
		if (!session->views.count)
		{
			recorded_session_close(session);
		}
		return;
	}

	if (!session->threadActive)
	{
		session->threadDesiredActive = true;
		session->threadActive = true;
		session->threadHandle = bbthread_create(recorded_session_read_thread, session);
		if (!session->threadHandle)
		{
			session->threadActive = false;
		}
	}

	view_init(view, session, autoClose);
}

void recorded_session_restart(recorded_session_t* session)
{
	for (u32 j = 0; j < session->views.count; ++j)
	{
		view_restart(session->views.data + j);
	}
	recorded_logs_reset(&session->logs);
	bba_free(session->partialLogs);
	bba_free(session->categories);
	bba_free(session->filenames);
	bba_free(session->threads);
	bba_free(session->pieInstances);
	bba_free(session->consoleAutocomplete);
	sb_reset(&session->consoleAutocomplete.request);
}

void recorded_session_close(recorded_session_t* session)
{
	u32 i, j;
	for (i = 0; i < s_sessions.count; ++i)
	{
		recorded_session_t* existing = *(s_sessions.data + i);
		if (!strcmp(existing->path, session->path))
		{
			if (session->threadActive)
			{
				session->threadDesiredActive = false;
			}
			else
			{
				bb_critical_section_shutdown(&session->incoming->cs);
				for (j = 0; j < session->views.count; ++j)
				{
					view_reset(session->views.data + j);
				}
				bba_free(session->views);
				recorded_logs_reset(&session->logs);
				bba_free(session->partialLogs);
				bba_free(session->categories);
				bba_free(session->filenames);
				bba_free(session->threads);
				bba_free(session->pieInstances);
				bba_free(session->consoleAutocomplete);
				sb_reset(&session->consoleAutocomplete.request);
				_aligned_free(session->incoming);
				if (session->outgoingMqId != mq_invalid_id())
				{
					mq_releaseref(session->outgoingMqId);
				}
				bb_free(session);
				bba_erase(s_sessions, i);
			}
			if (s_sessions.count == 0)
			{
				bba_free(s_sessions);
			}
			return;
		}
	}
}

void recorded_session_auto_close(const char* applicationName)
{
	u32 i;
	for (i = 0; i < s_sessions.count; ++i)
	{
		recorded_session_t* session = *(s_sessions.data + i);
		const char* sessionApplicationName = session->appInfo.packet.appInfo.applicationName;
		if (!strcmp(applicationName, sessionApplicationName))
		{
			u32 viewIndex;
			for (viewIndex = 0; viewIndex < session->views.count; ++viewIndex)
			{
				view_t* view = session->views.data + viewIndex;
				if (view->autoClose)
				{
					const recording_t* recording = recordings_find_by_path(session->path);
					if (!recording || !recording->active)
					{
						if (view->open)
						{
							view->open = false;
							if (view_config_write(view))
							{
								BB_LOG("View", "%s wrote config\n", view->session->appInfo.packet.appInfo.applicationName);
							}
							else
							{
								BB_ERROR("View", "%s failed to write config\n", view->session->appInfo.packet.appInfo.applicationName);
							}
						}
					}
				}
			}
		}
	}
}

void recorded_session_auto_close_all(void)
{
	u32 i;
	for (i = 0; i < s_sessions.count; ++i)
	{
		recorded_session_t* session = *(s_sessions.data + i);
		u32 viewIndex;
		for (viewIndex = 0; viewIndex < session->views.count; ++viewIndex)
		{
			view_t* view = session->views.data + viewIndex;
			if (view->autoClose)
			{
				const recording_t* recording = recordings_find_by_path(session->path);
				if (!recording || !recording->active)
				{
					if (view->open)
					{
						view->open = false;
						if (view_config_write(view))
						{
							BB_LOG("View", "%s wrote config\n", view->session->appInfo.packet.appInfo.applicationName);
						}
						else
						{
							BB_ERROR("View", "%s failed to write config\n", view->session->appInfo.packet.appInfo.applicationName);
						}
					}
				}
			}
		}
	}
}

void recorded_session_recording_stopped(const char* path)
{
	u32 i;
	for (i = 0; i < s_sessions.count; ++i)
	{
		recorded_session_t* session = *(s_sessions.data + i);
		if (!strcmp(session->path, path))
		{
			session->recordingActive = false;
			if (session->outgoingMqId != mq_invalid_id())
			{
				mq_releaseref(session->outgoingMqId);
				session->outgoingMqId = mq_invalid_id();
			}
			Imgui_Core_RequestRender();
		}
	}
}

static int CategoryCompare(const void* _a, const void* _b)
{
	const recorded_category_t* a = (const recorded_category_t*)_a;
	const recorded_category_t* b = (const recorded_category_t*)_b;
	return strcmp(a->categoryName, b->categoryName);
}

static void recorded_session_init_appinfo(recorded_session_t* session, bb_decoded_packet_t* decoded)
{
	view_config_add_categories_to_session(session);
	session->appInfo = *decoded;
	for (u32 i = 0; i < session->views.count; ++i)
	{
		view_t* view = session->views.data + i;
		view_init_appinfo(view);
	}
}

static void recorded_session_init_console_autocomplete_response(recorded_session_t* session, bb_decoded_packet_t* decoded)
{
	session->consoleAutocomplete.id = decoded->packet.consoleAutocompleteResponseHeader.id;
	session->consoleAutocomplete.expected = decoded->packet.consoleAutocompleteResponseHeader.total;
	if (!decoded->packet.consoleAutocompleteResponseHeader.reuse)
	{
		session->consoleAutocomplete.count = 0;
	}
}

static void recorded_session_add_console_autocomplete_entry(recorded_session_t* session, bb_decoded_packet_t* decoded)
{
	if (session->consoleAutocomplete.id != decoded->packet.consoleAutocompleteResponseEntry.id)
		return;

	bb_packet_console_autocomplete_response_entry_t* entry = bba_add(session->consoleAutocomplete, 1);
	if (entry)
	{
		entry->id = decoded->packet.consoleAutocompleteResponseEntry.id;
		entry->command = decoded->packet.consoleAutocompleteResponseEntry.command;
		bb_strncpy(entry->text, decoded->packet.consoleAutocompleteResponseEntry.text, sizeof(entry->text));
		bb_strncpy(entry->description, decoded->packet.consoleAutocompleteResponseEntry.description, sizeof(entry->description));
	}
}

static void recorded_session_echo_user_packet(recorded_session_t* session, bb_decoded_packet_t* decoded)
{
	if (!decoded->packet.userToServer.echo || session->appInfo.packet.appInfo.applicationGroup[0] == '\0')
		return;

	for (u32 i = 0; i < s_sessions.count; ++i)
	{
		recorded_session_t* other = *(s_sessions.data + i);
		if (other == session)
			continue;

		if (bb_stricmp(session->appInfo.packet.appInfo.applicationGroup, other->appInfo.packet.appInfo.applicationGroup))
			continue;

		if (other->outgoingMqId == mq_invalid_id())
			continue;

		mq_queue_userData(other->outgoingMqId, kBBPacketType_UserToClient, decoded->packet.userToServer.len, (const char*)decoded->packet.userToServer.data, decoded->packet.userToServer.len);
	}
}

void recorded_session_update(recorded_session_t* session)
{
	u64 start = bb_current_time_ms();
	bb_decoded_packet_t decoded;
	while (recorded_session_consume(session, &decoded))
	{
		recorded_thread_t* t = recorded_session_find_or_add_thread(session, &decoded);
		Imgui_Core_RequestRender();
		switch (decoded.type)
		{
		case kBBPacketType_Restart:
			recorded_session_restart(session);
			break;
		case kBBPacketType_AppInfo_v1:
		case kBBPacketType_AppInfo_v2:
		case kBBPacketType_AppInfo_v3:
		case kBBPacketType_AppInfo_v4:
		case kBBPacketType_AppInfo_v5:
			recorded_session_init_appinfo(session, &decoded);
			break;
		case kBBPacketType_CategoryId:
			recorded_session_add_category(session, &decoded);
			break;
		case kBBPacketType_LogTextPartial:
			recorded_session_add_partial_log(session, &decoded, t);
			break;
		case kBBPacketType_LogText_v1:
		case kBBPacketType_LogText_v2:
		case kBBPacketType_LogText:
			recorded_session_add_log(session, &decoded, t);
			break;
		case kBBPacketType_FileId:
			recorded_session_add_fileid(session, &decoded);
			break;
		case kBBPacketType_ThreadName:
		case kBBPacketType_ThreadStart:
			break;
		case kBBPacketType_ThreadEnd:
			if (t && !t->endTime)
			{
				t->endTime = decoded.header.timestamp;
			}
			break;
		case kBBPacketType_ConsoleAutocompleteResponseHeader:
			recorded_session_init_console_autocomplete_response(session, &decoded);
			break;
		case kBBPacketType_ConsoleAutocompleteResponseEntry:
			recorded_session_add_console_autocomplete_entry(session, &decoded);
			break;
		case kBBPacketType_FrameNumber:
			session->currentFrameNumber = decoded.packet.frameNumber.frameNumber;
			break;
		case kBBPacketType_Invalid:
		case kBBPacketType_FrameEnd:
		case kBBPacketType_ConsoleCommand:
		case kBBPacketType_UserToClient:
		case kBBPacketType_StopRecording:
		case kBBPacketType_RecordingInfo:
		case kBBPacketType_ConsoleAutocompleteRequest:
			break;
		case kBBPacketType_UserToServer:
			recorded_session_echo_user_packet(session, &decoded);
			break;
		default:
			break;
		}

		// if we're spinning through data on the reading thread,
		// don't lock up the UI processing that data - we can just
		// throttle instead.
		if (bb_current_time_ms() - start > 16)
		{
			OutputDebugStringA("throttling read\n");
			break;
		}
	}
	if (session->failedToDeserialize && !session->shownDeserializationMessageBox)
	{
		session->shownDeserializationMessageBox = true;

		messageBox mb = { BB_EMPTY_INITIALIZER };
		sdict_add_raw(&mb.data, "title", u8"\uf06a Data Corruption");
		sdict_add_raw(&mb.data, "text", va("Failed to deserialize %s", session->path));
		sdict_add_raw(&mb.data, "button1", "Ok");

		if (session->views.count)
		{
			for (u32 i = 0; i < session->views.count; ++i)
			{
				view_t* view = session->views.data + i;
				mb_queue(mb, &view->messageboxes);
			}
		}
		else
		{
			mb_queue(mb, NULL);
		}
	}
}

recorded_session_t* recorded_session_find(const char* path)
{
	u32 i;
	for (i = 0; i < s_sessions.count; ++i)
	{
		recorded_session_t* existing = *(s_sessions.data + i);
		if (!strcmp(existing->path, path))
		{
			return existing;
		}
	}
	return NULL;
}

u32 recorded_session_count(void)
{
	return s_sessions.count;
}

recorded_session_t* recorded_session_get(u32 index)
{
	if (s_sessions.count > index)
	{
		return *(s_sessions.data + index);
	}
	else
	{
		return NULL;
	}
}

static void recorded_session_add_category_from_config(recorded_session_t* session, const view_config_category_t* configCategory)
{
	const char* categoryName = sb_get(&configCategory->name);
	recorded_category_t* c = recorded_session_find_category_by_name(session, categoryName);
	if (c)
	{
		return;
	}

	c = bba_add(session->categories, 1);
	if (c)
	{
		bb_strncpy(c->categoryName, categoryName, sizeof(c->categoryName));
		Fonts_CacheGlyphs(c->categoryName);
		for (u32 viewIndex = 0; viewIndex < session->views.count; ++viewIndex)
		{
			view_add_category(session->views.data + viewIndex, c, configCategory);
		}
		qsort(session->categories.data, session->categories.count, sizeof(session->categories.data[0]), CategoryCompare);
	}
}

static void recorded_session_add_category(recorded_session_t* session, bb_decoded_packet_t* decoded)
{
	recorded_category_t* c = recorded_session_find_category_by_name(session, decoded->packet.categoryId.name);
	if (c)
	{
		c->id = decoded->packet.categoryId.id;
		for (u32 viewIndex = 0; viewIndex < session->views.count; ++viewIndex)
		{
			view_update_category_id(session->views.data + viewIndex, c);
		}
		return;
	}

	c = bba_add(session->categories, 1);
	if (c)
	{
		c->id = decoded->packet.categoryId.id;
		bb_strncpy(c->categoryName, decoded->packet.categoryId.name, sizeof(c->categoryName));
		Fonts_CacheGlyphs(c->categoryName);
		for (u32 viewIndex = 0; viewIndex < session->views.count; ++viewIndex)
		{
			view_add_category(session->views.data + viewIndex, c, NULL);
		}
		qsort(session->categories.data, session->categories.count, sizeof(session->categories.data[0]), CategoryCompare);
	}
}

void recorded_session_add_config_category(recorded_session_t* session, const view_config_category_t* configCategory)
{
	const char* categoryName = sb_get(&configCategory->name);
	recorded_category_t* rc = recorded_session_find_category_by_name(session, categoryName);
	if (!rc)
	{
		bb_decoded_packet_t decoded = { BB_EMPTY_INITIALIZER };
		bb_strncpy(decoded.packet.categoryId.name, categoryName, sizeof(decoded.packet.categoryId.name));
		recorded_session_add_category_from_config(session, configCategory);
	}
}

recorded_category_t* recorded_session_find_category_by_name(recorded_session_t* session, const char* categoryName)
{
	u32 categoryIndex;
	for (categoryIndex = 0; categoryIndex < session->categories.count; ++categoryIndex)
	{
		recorded_category_t* category = session->categories.data + categoryIndex;
		if (!strcmp(category->categoryName, categoryName))
		{
			return category;
		}
	}
	return NULL;
}

recorded_category_t* recorded_session_find_category(recorded_session_t* session, u32 categoryId)
{
	u32 categoryIndex;
	for (categoryIndex = 0; categoryIndex < session->categories.count; ++categoryIndex)
	{
		recorded_category_t* category = session->categories.data + categoryIndex;
		if (category->id == categoryId)
		{
			return category;
		}
	}
	return NULL;
}

static void clear_to_zero(void* zp, size_t bytes)
{
	u8* p = zp;
	while (bytes--)
	{
		*p++ = 0;
	}
}

static void recorded_session_add_partial_log(recorded_session_t* session, bb_decoded_packet_t* decoded, recorded_thread_t* t)
{
	size_t len = strlen(decoded->packet.logText.text);
	if (len > 0 && decoded->packet.logText.text[len - 1] == '\n')
	{
		// partial ends in a '\n'.  Check if we have too long a buffer queued up, and treat it as a full log instead if so.
		size_t queuedLen = len;
		for (u32 i = 0; i < session->partialLogs.count; ++i)
		{
			const bb_decoded_packet_t* partial = session->partialLogs.data + i;
			if (partial->header.threadId == decoded->header.threadId)
			{
				queuedLen += strlen(partial->packet.logText.text);
			}
		}

		if (queuedLen > 16 * 1024)
		{
			recorded_session_add_log(session, decoded, t);
			return;
		}
	}

	bba_push(session->partialLogs, *decoded);
}

static void recorded_session_add_log(recorded_session_t* session, bb_decoded_packet_t* decoded, recorded_thread_t* t)
{
	sb_clear(&s_reconstructedLogText);
	for (u32 i = 0; i < session->partialLogs.count; ++i)
	{
		const bb_decoded_packet_t* partial = session->partialLogs.data + i;
		if (partial->header.threadId == decoded->header.threadId)
		{
			sb_append(&s_reconstructedLogText, partial->packet.logText.text);
		}
	}
	sb_append(&s_reconstructedLogText, decoded->packet.logText.text);

	recorded_log_lines_t recordedLogLines = { BB_EMPTY_INITIALIZER };
	span_t linesCursor = { s_reconstructedLogText.data, s_reconstructedLogText.data + s_reconstructedLogText.count - 1 };
	for (span_t line = tokenizeLine(&linesCursor); line.start; line = tokenizeLine(&linesCursor))
	{
		recorded_log_line_t *recordedLogLine = bba_add(recordedLogLines, 1);
		if (recordedLogLine)
		{
			recordedLogLine->offset = (u32)(line.start - s_reconstructedLogText.data);
			recordedLogLine->len = (u32)(line.end - line.start);
		}
	}
	if (!recordedLogLines.count)
	{
		bba_free(recordedLogLines);
		return;
	}

	u32 categoryId = decoded->packet.logText.categoryId;
	recorded_log_t** plog;
	recorded_log_t* log;
	recorded_category_t* category = recorded_session_find_category(session, categoryId);
	if (category)
	{
		if (decoded->packet.logText.level < kBBLogLevel_Count)
		{
			++category->logCount[decoded->packet.logText.level];
		}
	}
	if (decoded->packet.logText.level < kBBLogLevel_Count)
	{
		recorded_filename_t* file = recorded_session_find_filename(session, decoded->header.fileId);
		if (file)
		{
			++file->logCount[decoded->packet.logText.level];
		}
		recorded_pieInstance_t* pieInstance = recorded_session_find_or_add_pieInstance(session, decoded->packet.logText.pieInstance);
		if (pieInstance)
		{
			++pieInstance->logCount[decoded->packet.logText.level];
		}
		++t->logCount[decoded->packet.logText.level];
	}
	plog = bba_add(session->logs, 1);
	if (plog)
	{
		size_t textLen = sb_len(&s_reconstructedLogText);
		size_t preTextSize = (u8*)decoded->packet.logText.text - (u8*)decoded;
		size_t decodedSize = preTextSize + textLen + 1;
		size_t logSize = decodedSize + offsetof(recorded_log_t, packet);
		*plog = bb_malloc(logSize);
		log = *plog;
		if (log)
		{
			log->lines = recordedLogLines;
			//Fonts_CacheGlyphs_Range(text, text + textLen);
			log->sessionLogIndex = session->logs.count - 1;
			log->frameNumber = session->currentFrameNumber;
			memcpy(&log->packet, decoded, preTextSize);
			memcpy(log->packet.packet.logText.text, sb_get(&s_reconstructedLogText), textLen + 1);
			for (u32 i = 0; i < session->views.count; ++i)
			{
				view_add_log(session->views.data + i, log);
			}
		}
		else
		{
			--session->logs.count;
			bba_free(recordedLogLines);
		}
		for (u32 i = 0; i < session->partialLogs.count;)
		{
			const bb_decoded_packet_t* partial = session->partialLogs.data + i;
			if (partial->header.threadId == decoded->header.threadId)
			{
				u32 end = i + 1;
				for (u32 j = end; j < session->partialLogs.count; ++j)
				{
					const bb_decoded_packet_t* endPacket = session->partialLogs.data + j;
					if (endPacket->header.threadId == decoded->header.threadId)
					{
						end = j + 1;
					}
					else
					{
						break;
					}
				}
				bba_erase_num(session->partialLogs, i, end - i);
			}
			else
			{
				++i;
			}
		}
	}
	else
	{
		bba_free(recordedLogLines);
	}
}

recorded_filename_t* recorded_session_find_filename(recorded_session_t* session, u32 fileId)
{
	u32 i;
	for (i = 0; i < session->filenames.count; ++i)
	{
		recorded_filename_t* filename = session->filenames.data + i;
		if (filename->id == fileId)
		{
			return filename;
		}
	}
	return NULL;
}

static void recorded_session_add_fileid(recorded_session_t* session, bb_decoded_packet_t* decoded)
{
	recorded_filename_t* entry = bba_add(session->filenames, 1);
	if (entry)
	{
		u32 i;
		entry->id = decoded->packet.fileId.id;
		bb_strncpy(entry->path, decoded->packet.fileId.name, sizeof(entry->path));
		Fonts_CacheGlyphs(entry->path);
		for (i = 0; i < session->views.count; ++i)
		{
			view_add_file(session->views.data + i, entry);
		}
	}
}

recorded_thread_t* recorded_session_find_thread(recorded_session_t* session, u64 threadId)
{
	u32 i;
	for (i = 0; i < session->threads.count; ++i)
	{
		recorded_thread_t* t = session->threads.data + i;
		if (t->id == threadId)
		{
			return t;
		}
	}
	return NULL;
}

static int ThreadCompare(const void* _a, const void* _b)
{
	const recorded_thread_t* a = (const recorded_thread_t*)_a;
	const recorded_thread_t* b = (const recorded_thread_t*)_b;
	int res = strcmp(a->threadName, b->threadName);
	if (res)
	{
		return res;
	}
	else
	{
		return a->id > b->id ? 1 : -1;
	}
}
static void recorded_session_sort_threads(recorded_session_t* session)
{
	qsort(session->threads.data, session->threads.count, sizeof(session->threads.data[0]), ThreadCompare);
}

static recorded_thread_t* recorded_session_find_or_add_thread(recorded_session_t* session, bb_decoded_packet_t* decoded)
{
	u32 i;
	u32 viewIndex;
	u64 threadId = decoded->header.threadId;
	recorded_thread_t* t;
	for (i = 0; i < session->threads.count; ++i)
	{
		t = session->threads.data + i;
		if (t->id == threadId)
		{
			BB_WARNING_PUSH(4061); // warning C4061: enumerator 'kBBDiscoveryPacketType_Invalid' in switch of enum 'bb_discovery_packet_type_e' is not explicitly handled by a case label
			switch (decoded->type)
			{
			case kBBPacketType_ThreadStart:
				bb_strncpy(t->threadName, decoded->packet.threadStart.text, sizeof(t->threadName));
				Fonts_CacheGlyphs(t->threadName);
				for (viewIndex = 0; viewIndex < session->views.count; ++viewIndex)
				{
					view_t* view = session->views.data + viewIndex;
					view_set_thread_name(view, t->id, t->threadName);
				}
				recorded_session_sort_threads(session);
				break;
			case kBBPacketType_ThreadName:
				bb_strncpy(t->threadName, decoded->packet.threadName.text, sizeof(t->threadName));
				Fonts_CacheGlyphs(t->threadName);
				for (viewIndex = 0; viewIndex < session->views.count; ++viewIndex)
				{
					view_t* view = session->views.data + viewIndex;
					view_set_thread_name(view, t->id, t->threadName);
				}
				recorded_session_sort_threads(session);
				break;
			default: break;
			}
			BB_WARNING_POP;
			return t;
		}
	}
	t = bba_add(session->threads, 1);
	if (t)
	{
		t->id = threadId;
		t->startTime = decoded->header.timestamp;
		BB_WARNING_PUSH(4061); // warning C4061: enumerator 'kBBDiscoveryPacketType_Invalid' in switch of enum 'bb_discovery_packet_type_e' is not explicitly handled by a case label
		switch (decoded->type)
		{
		case kBBPacketType_ThreadStart:
			bb_strncpy(t->threadName, decoded->packet.threadStart.text, sizeof(t->threadName));
			Fonts_CacheGlyphs(t->threadName);
			break;
		case kBBPacketType_ThreadName:
			bb_strncpy(t->threadName, decoded->packet.threadName.text, sizeof(t->threadName));
			Fonts_CacheGlyphs(t->threadName);
			break;
		default:
			bb_strncpy(t->threadName, va("thread_%" PRIu64, threadId), sizeof(t->threadName));
			break;
		}
		BB_WARNING_POP;
		for (i = 0; i < session->views.count; ++i)
		{
			view_add_thread(session->views.data + i, t);
		}
		recorded_session_sort_threads(session);
	}
	return t;
}

recorded_pieInstance_t* recorded_session_find_pieInstance(recorded_session_t* session, s32 pieInstance)
{
	for (u32 i = 0; i < session->pieInstances.count; ++i)
	{
		recorded_pieInstance_t* sessionPieInstance = session->pieInstances.data + i;
		if (sessionPieInstance->pieInstance == pieInstance)
		{
			return sessionPieInstance;
		}
	}
	return NULL;
}

static recorded_pieInstance_t* recorded_session_find_or_add_pieInstance(recorded_session_t* session, s32 pieInstance)
{
	for (u32 i = 0; i < session->pieInstances.count; ++i)
	{
		recorded_pieInstance_t* sessionPieInstance = session->pieInstances.data + i;
		if (sessionPieInstance->pieInstance == pieInstance)
		{
			return sessionPieInstance;
		}
	}

	recorded_pieInstance_t* sessionPieInstance = bba_add(session->pieInstances, 1);
	if (sessionPieInstance)
	{
		sessionPieInstance->pieInstance = pieInstance;
	}
	for (u32 viewIndex = 0; viewIndex < session->views.count; ++viewIndex)
	{
		view_add_pieInstance(session->views.data + viewIndex, pieInstance);
	}
	return sessionPieInstance;
}

char* recorded_session_get_thread_name(recorded_session_t* session, u64 threadId)
{
	recorded_thread_t* t = recorded_session_find_thread(session, threadId);
	return t ? t->threadName : va("%d", threadId);
}

const char* recorded_session_get_filename(recorded_session_t* session, u32 fileId)
{
	recorded_filename_t* f = recorded_session_find_filename(session, fileId);
	const char* path = (f) ? f->path : va("%d", fileId);
	return path_get_filename(path);
}

const char* recorded_session_get_category_name(recorded_session_t* session, u32 categoryId)
{
	recorded_category_t* category = recorded_session_find_category(session, categoryId);
	return category ? category->categoryName : "";
}
