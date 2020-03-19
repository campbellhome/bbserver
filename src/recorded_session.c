// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "recorded_session.h"
#include "fonts.h"
#include "imgui_core.h"
#include "message_box.h"
#include "message_queue.h"
#include "recorded_session_thread.h"
#include "recordings.h"
#include "span.h"
#include "tokenize.h"
#include "va.h"
#include "view.h"
#include "view_config.h"

#include "bb.h"
#include "bb_array.h"
#include "bb_packet.h"
#include "bb_string.h"
#include "bb_thread.h"
#include "bb_time.h"

#include "bb_wrap_stdio.h"
#include <stdlib.h>

static void recorded_session_add_category(recorded_session_t *session, bb_decoded_packet_t *decoded);
static void recorded_session_add_partial_log(recorded_session_t *session, bb_decoded_packet_t *decoded);
static void recorded_session_add_log(recorded_session_t *session, bb_decoded_packet_t *decoded, recorded_thread_t *t);
static void recorded_session_add_fileid(recorded_session_t *session, bb_decoded_packet_t *decoded);
static recorded_thread_t *recorded_session_find_or_add_thread(recorded_session_t *session, bb_decoded_packet_t *decoded);
static recorded_pieInstance_t *recorded_session_find_or_add_pieInstance(recorded_session_t *session, u32 pieInstance);

static sb_t s_reconstructedLogText;

typedef struct recorded_sessions_s {
	u32 count;
	u32 allocated;
	recorded_session_t **data; // can't allocate recorded_session_t contiguously because other threads need a stable address
} recorded_sessions_t;
static recorded_sessions_t s_sessions;

static void recorded_logs_reset(recorded_logs_t *logs)
{
	for(u32 i = 0; i < logs->count; ++i) {
		free(logs->data[i]);
	}
	bba_free(*logs);
}

void recorded_session_shutdown(void)
{
	recorded_session_t *session;
	while((session = recorded_session_get(0)) != NULL) {
		while(session->views.count) {
			u32 viewIndex = session->views.count - 1;
			view_t *view = session->views.data + viewIndex;
			view_reset(view);
			bba_erase(session->views, viewIndex);
		}
		recorded_session_close(session);
	}
	sb_reset(&s_reconstructedLogText);
}

void recorded_session_open(const char *path, const char *applicationFilename, b8 autoClose, b32 recordingActive, u32 outgoingMqId)
{
	if(g_config.autoCloseAll) {
		recorded_session_auto_close_all();
	} else {
		recorded_session_auto_close(applicationFilename);
	}

	view_t *view;
	recorded_session_t *session = recorded_session_find(path);
	if(!session) {
		recorded_session_t **holder = bba_add(s_sessions, 1);
		if(!holder)
			return;
		*holder = session = (recorded_session_t *)malloc(sizeof(recorded_session_t));
		if(!session) {
			--s_sessions.count;
			return;
		}
		memset(session, 0, sizeof(*session));
		session->incoming = _aligned_malloc(sizeof(session_message_queue_t), 8);
		if(!session->incoming) {
			free(session);
			--s_sessions.count;
			return;
		}
		memset(session->incoming, 0, sizeof(*session->incoming));
		bb_strncpy(session->path, path, sizeof(session->path));
		bb_strncpy(session->applicationFilename, applicationFilename, sizeof(session->applicationFilename));
		bb_critical_section_init(&session->incoming->cs);
		session->appInfo.packet.appInfo.millisPerTick = 1.0;
		session->recordingActive = (b8)recordingActive;
		if(outgoingMqId == mq_invalid_id()) {
			session->outgoingMqId = mq_invalid_id();
		} else {
			session->outgoingMqId = mq_addref(outgoingMqId);
		}
	}

	view = bba_add(session->views, 1);
	if(!view) {
		if(!session->views.count) {
			recorded_session_close(session);
		}
		return;
	}

	if(!session->threadActive) {
		session->threadDesiredActive = true;
		session->threadActive = true;
		session->threadHandle = bbthread_create(recorded_session_read_thread, session);
		if(!session->threadHandle) {
			session->threadActive = false;
		}
	}

	view_init(view, session, autoClose);
}

void recorded_session_restart(recorded_session_t *session)
{
	for(u32 j = 0; j < session->views.count; ++j) {
		view_restart(session->views.data + j);
	}
	recorded_logs_reset(&session->logs);
	bba_free(session->partialLogs);
	bba_free(session->categories);
	bba_free(session->filenames);
	bba_free(session->threads);
	bba_free(session->pieInstances);
}

void recorded_session_close(recorded_session_t *session)
{
	u32 i, j;
	for(i = 0; i < s_sessions.count; ++i) {
		recorded_session_t *existing = *(s_sessions.data + i);
		if(!strcmp(existing->path, session->path)) {
			if(session->threadActive) {
				session->threadDesiredActive = false;
			} else {
				bb_critical_section_shutdown(&session->incoming->cs);
				for(j = 0; j < session->views.count; ++j) {
					view_reset(session->views.data + j);
				}
				bba_free(session->views);
				recorded_logs_reset(&session->logs);
				bba_free(session->partialLogs);
				bba_free(session->categories);
				bba_free(session->filenames);
				bba_free(session->threads);
				bba_free(session->pieInstances);
				_aligned_free(session->incoming);
				if(session->outgoingMqId != mq_invalid_id()) {
					mq_releaseref(session->outgoingMqId);
				}
				free(session);
				bba_erase(s_sessions, i);
			}
			if(s_sessions.count == 0) {
				bba_free(s_sessions);
			}
			return;
		}
	}
}

void recorded_session_auto_close(const char *applicationName)
{
	u32 i;
	for(i = 0; i < s_sessions.count; ++i) {
		recorded_session_t *session = *(s_sessions.data + i);
		const char *sessionApplicationName = session->appInfo.packet.appInfo.applicationName;
		if(!strcmp(applicationName, sessionApplicationName)) {
			u32 viewIndex;
			for(viewIndex = 0; viewIndex < session->views.count; ++viewIndex) {
				view_t *view = session->views.data + viewIndex;
				if(view->autoClose) {
					const recording_t *recording = recordings_find_by_path(session->path);
					if(!recording || !recording->active) {
						if(view->open) {
							view->open = false;
							if(view_config_write(view)) {
								BB_LOG("View", "%s wrote config\n", view->session->appInfo.packet.appInfo.applicationName);
							} else {
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
	for(i = 0; i < s_sessions.count; ++i) {
		recorded_session_t *session = *(s_sessions.data + i);
		u32 viewIndex;
		for(viewIndex = 0; viewIndex < session->views.count; ++viewIndex) {
			view_t *view = session->views.data + viewIndex;
			if(view->autoClose) {
				const recording_t *recording = recordings_find_by_path(session->path);
				if(!recording || !recording->active) {
					if(view->open) {
						view->open = false;
						if(view_config_write(view)) {
							BB_LOG("View", "%s wrote config\n", view->session->appInfo.packet.appInfo.applicationName);
						} else {
							BB_ERROR("View", "%s failed to write config\n", view->session->appInfo.packet.appInfo.applicationName);
						}
					}
				}
			}
		}
	}
}

void recorded_session_recording_stopped(const char *path)
{
	u32 i;
	for(i = 0; i < s_sessions.count; ++i) {
		recorded_session_t *session = *(s_sessions.data + i);
		if(!strcmp(session->path, path)) {
			session->recordingActive = false;
			if(session->outgoingMqId != mq_invalid_id()) {
				mq_releaseref(session->outgoingMqId);
				session->outgoingMqId = mq_invalid_id();
			}
			Imgui_Core_RequestRender();
		}
	}
}

static int CategoryCompare(const void *_a, const void *_b)
{
	const recorded_category_t *a = (const recorded_category_t *)_a;
	const recorded_category_t *b = (const recorded_category_t *)_b;
	return strcmp(a->categoryName, b->categoryName);
}

static void recorded_session_init_appinfo(recorded_session_t *session, bb_decoded_packet_t *decoded)
{
	view_config_add_categories_to_session(session);
	session->appInfo = *decoded;
	for(u32 i = 0; i < session->views.count; ++i) {
		view_t *view = session->views.data + i;
		view_init_appinfo(view);
	}
}

void recorded_session_update(recorded_session_t *session)
{
	u64 start = bb_current_time_ms();
	bb_decoded_packet_t decoded;
	while(recorded_session_consume(session, &decoded)) {
		recorded_thread_t *t = recorded_session_find_or_add_thread(session, &decoded);
		Imgui_Core_RequestRender();
		switch(decoded.type) {
		case kBBPacketType_Restart:
			recorded_session_restart(session);
			break;
		case kBBPacketType_AppInfo_v1:
		case kBBPacketType_AppInfo_v2:
		case kBBPacketType_AppInfo_v3:
		case kBBPacketType_AppInfo:
			recorded_session_init_appinfo(session, &decoded);
			break;
		case kBBPacketType_CategoryId:
			recorded_session_add_category(session, &decoded);
			break;
		case kBBPacketType_LogTextPartial:
			recorded_session_add_partial_log(session, &decoded);
			break;
		case kBBPacketType_LogText_v1:
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
			if(t && !t->endTime) {
				t->endTime = decoded.header.timestamp;
			}
			break;
		default:
			break;
		}

		// if we're spinning through data on the reading thread,
		// don't lock up the UI processing that data - we can just
		// throttle instead.
		if(bb_current_time_ms() - start > 16) {
			OutputDebugStringA("throttling read\n");
			break;
		}
	}
	if(session->failedToDeserialize && !session->shownDeserializationMessageBox) {
		session->shownDeserializationMessageBox = true;

		messageBox mb = { BB_EMPTY_INITIALIZER };
		sdict_add_raw(&mb.data, "title", u8"\uf06a Data Corruption");
		sdict_add_raw(&mb.data, "text", va("Failed to deserialize %s", session->path));
		sdict_add_raw(&mb.data, "button1", "Ok");

		if(session->views.count) {
			for(u32 i = 0; i < session->views.count; ++i) {
				view_t *view = session->views.data + i;
				mb_queue(mb, &view->messageboxes);
			}
		} else {
			mb_queue(mb, NULL);
		}
	}
}

recorded_session_t *recorded_session_find(const char *path)
{
	u32 i;
	for(i = 0; i < s_sessions.count; ++i) {
		recorded_session_t *existing = *(s_sessions.data + i);
		if(!strcmp(existing->path, path)) {
			return existing;
		}
	}
	return NULL;
}

u32 recorded_session_count(void)
{
	return s_sessions.count;
}

recorded_session_t *recorded_session_get(u32 index)
{
	if(s_sessions.count > index) {
		return *(s_sessions.data + index);
	} else {
		return NULL;
	}
}

static u32 recorded_session_update_category_parent(recorded_session_t *session, u32 startIndex, u32 parentIndex)
{
	u32 endIndex = startIndex + 1;
	recorded_categories_t *categories = &session->categories;
	recorded_category_t *category = categories->data + startIndex;
	category->parentIndex = parentIndex;

	while(endIndex < categories->count) {
		recorded_category_t *subCategory = categories->data + endIndex;
		if(subCategory->depth <= category->depth)
			break;
		++endIndex;
	}

	if(startIndex + 1 != endIndex) {
		u32 index = startIndex + 1;
		while(index < endIndex) {
			index = recorded_session_update_category_parent(session, index, startIndex);
		}
	}
	return endIndex;
}

static void recorded_session_add_category_from_config(recorded_session_t *session, const view_config_category_t *configCategory)
{
	const char *categoryName = sb_get(&configCategory->name);
	recorded_category_t *c = recorded_session_find_category_by_name(session, categoryName);
	if(c) {
		return;
	}

	c = bba_add(session->categories, 1);
	if(c) {
		u32 viewIndex;
		u32 index = 0;
		const char *s;
		bb_strncpy(c->categoryName, categoryName, sizeof(c->categoryName));
		Fonts_CacheGlyphs(c->categoryName);
		for(s = c->categoryName; *s; ++s) {
			if(s[0] == ':' && s[1] == ':') {
				++c->depth;
				++s; // ::: only counts as 1 match
			}
		}
		for(viewIndex = 0; viewIndex < session->views.count; ++viewIndex) {
			view_add_category(session->views.data + viewIndex, c, configCategory);
		}
		qsort(session->categories.data, session->categories.count, sizeof(session->categories.data[0]), CategoryCompare);
		while(index < session->categories.count) {
			index = recorded_session_update_category_parent(session, index, session->categories.count);
		}
	}
}

static void recorded_session_add_category(recorded_session_t *session, bb_decoded_packet_t *decoded)
{
	recorded_category_t *c = recorded_session_find_category_by_name(session, decoded->packet.categoryId.name);
	if(c) {
		c->id = decoded->packet.categoryId.id;
		for(u32 viewIndex = 0; viewIndex < session->views.count; ++viewIndex) {
			view_update_category_id(session->views.data + viewIndex, c);
		}
		return;
	}

	c = bba_add(session->categories, 1);
	if(c) {
		u32 viewIndex;
		u32 index = 0;
		const char *s;
		c->id = decoded->packet.categoryId.id;
		bb_strncpy(c->categoryName, decoded->packet.categoryId.name, sizeof(c->categoryName));
		Fonts_CacheGlyphs(c->categoryName);
		for(s = c->categoryName; *s; ++s) {
			if(s[0] == ':' && s[1] == ':') {
				++c->depth;
				++s; // ::: only counts as 1 match
			}
		}
		for(viewIndex = 0; viewIndex < session->views.count; ++viewIndex) {
			view_add_category(session->views.data + viewIndex, c, NULL);
		}
		qsort(session->categories.data, session->categories.count, sizeof(session->categories.data[0]), CategoryCompare);
		while(index < session->categories.count) {
			index = recorded_session_update_category_parent(session, index, session->categories.count);
		}
	}
}

void recorded_session_add_config_category(recorded_session_t *session, const view_config_category_t *configCategory)
{
	const char *categoryName = sb_get(&configCategory->name);
	recorded_category_t *rc = recorded_session_find_category_by_name(session, categoryName);
	if(!rc) {
		bb_decoded_packet_t decoded = { BB_EMPTY_INITIALIZER };
		bb_strncpy(decoded.packet.categoryId.name, categoryName, sizeof(decoded.packet.categoryId.name));
		recorded_session_add_category_from_config(session, configCategory);
	}
}

recorded_category_t *recorded_session_find_category_by_name(recorded_session_t *session, const char *categoryName)
{
	u32 categoryIndex;
	for(categoryIndex = 0; categoryIndex < session->categories.count; ++categoryIndex) {
		recorded_category_t *category = session->categories.data + categoryIndex;
		if(!strcmp(category->categoryName, categoryName)) {
			return category;
		}
	}
	return NULL;
}

recorded_category_t *recorded_session_find_category(recorded_session_t *session, u32 categoryId)
{
	u32 categoryIndex;
	for(categoryIndex = 0; categoryIndex < session->categories.count; ++categoryIndex) {
		recorded_category_t *category = session->categories.data + categoryIndex;
		if(category->id == categoryId) {
			return category;
		}
	}
	return NULL;
}

static void clear_to_zero(void *zp, size_t bytes)
{
	u8 *p = zp;
	while(bytes--) {
		*p++ = 0;
	}
}

static void recorded_session_add_partial_log(recorded_session_t *session, bb_decoded_packet_t *decoded)
{
	bba_push(session->partialLogs, *decoded);
}

static void recorded_session_add_log(recorded_session_t *session, bb_decoded_packet_t *decoded, recorded_thread_t *t)
{
	sb_clear(&s_reconstructedLogText);
	for(u32 i = 0; i < session->partialLogs.count; ++i) {
		const bb_decoded_packet_t *partial = session->partialLogs.data + i;
		if(partial->header.threadId == decoded->header.threadId) {
			sb_append(&s_reconstructedLogText, partial->packet.logText.text);
		}
	}
	sb_append(&s_reconstructedLogText, decoded->packet.logText.text);
	const char *text = sb_get(&s_reconstructedLogText);

	u32 numLines = 0;
	span_t linesCursor = span_from_string(text);
	for(span_t line = tokenizeLine(&linesCursor); line.start; line = tokenizeLine(&linesCursor)) {
		++numLines;
	}
	if(!numLines) {
		return;
	}

	u32 categoryId = decoded->packet.logText.categoryId;
	recorded_log_t **plog;
	recorded_log_t *log;
	recorded_category_t *category = recorded_session_find_category(session, categoryId);
	if(category) {
		if(decoded->packet.logText.level < kBBLogLevel_Count) {
			u32 parentIndex = category->parentIndex;
			++category->logCount[decoded->packet.logText.level];
			++category->logCountIncludingChildren[decoded->packet.logText.level];
			while(parentIndex < session->categories.count) {
				recorded_category_t *parent = session->categories.data + parentIndex;
				++parent->logCountIncludingChildren[decoded->packet.logText.level];
				parentIndex = parent->parentIndex;
			}
		}
	}
	if(decoded->packet.logText.level < kBBLogLevel_Count) {
		recorded_filename_t *file = recorded_session_find_filename(session, decoded->header.fileId);
		if(file) {
			++file->logCount[decoded->packet.logText.level];
		}
		recorded_pieInstance_t *pieInstance = recorded_session_find_or_add_pieInstance(session, decoded->packet.logText.pieInstance);
		if(pieInstance) {
			++pieInstance->logCount[decoded->packet.logText.level];
		}
		++t->logCount[decoded->packet.logText.level];
	}
	plog = bba_add(session->logs, 1);
	if(plog) {
		size_t textLen = strlen(text);
		size_t preTextSize = (u8 *)decoded->packet.logText.text - (u8 *)decoded;
		size_t decodedSize = preTextSize + textLen + 1;
		size_t logSize = decodedSize + offsetof(recorded_log_t, packet);
		*plog = malloc(logSize);
		log = *plog;
		if(log) {
			Fonts_CacheGlyphs(text);
			log->sessionLogIndex = session->logs.count - 1;
			log->numLines = numLines;
			memcpy(&log->packet, decoded, preTextSize);
			bb_strncpy(log->packet.packet.logText.text, text, textLen + 1);
			for(u32 i = 0; i < session->views.count; ++i) {
				view_add_log(session->views.data + i, log);
			}
		} else {
			--session->logs.count;
		}
		for(u32 i = 0; i < session->partialLogs.count;) {
			const bb_decoded_packet_t *partial = session->partialLogs.data + i;
			if(partial->header.threadId == decoded->header.threadId) {
				bba_erase(session->partialLogs, i);
			} else {
				++i;
			}
		}
	}
}

recorded_filename_t *recorded_session_find_filename(recorded_session_t *session, u32 fileId)
{
	u32 i;
	for(i = 0; i < session->filenames.count; ++i) {
		recorded_filename_t *filename = session->filenames.data + i;
		if(filename->id == fileId) {
			return filename;
		}
	}
	return NULL;
}

static void recorded_session_add_fileid(recorded_session_t *session, bb_decoded_packet_t *decoded)
{
	recorded_filename_t *entry = bba_add(session->filenames, 1);
	if(entry) {
		u32 i;
		entry->id = decoded->packet.fileId.id;
		bb_strncpy(entry->path, decoded->packet.fileId.name, sizeof(entry->path));
		Fonts_CacheGlyphs(entry->path);
		for(i = 0; i < session->views.count; ++i) {
			view_add_file(session->views.data + i, entry);
		}
	}
}

recorded_thread_t *recorded_session_find_thread(recorded_session_t *session, u64 threadId)
{
	u32 i;
	for(i = 0; i < session->threads.count; ++i) {
		recorded_thread_t *t = session->threads.data + i;
		if(t->id == threadId) {
			return t;
		}
	}
	return NULL;
}

static int ThreadCompare(const void *_a, const void *_b)
{
	const recorded_thread_t *a = (const recorded_thread_t *)_a;
	const recorded_thread_t *b = (const recorded_thread_t *)_b;
	int res = strcmp(a->threadName, b->threadName);
	if(res) {
		return res;
	} else {
		return a->id > b->id ? 1 : -1;
	}
}
static void recorded_session_sort_threads(recorded_session_t *session)
{
	qsort(session->threads.data, session->threads.count, sizeof(session->threads.data[0]), ThreadCompare);
}

static recorded_thread_t *recorded_session_find_or_add_thread(recorded_session_t *session, bb_decoded_packet_t *decoded)
{
	u32 i;
	u32 viewIndex;
	u64 threadId = decoded->header.threadId;
	recorded_thread_t *t;
	for(i = 0; i < session->threads.count; ++i) {
		t = session->threads.data + i;
		if(t->id == threadId) {
			switch(decoded->type) {
			case kBBPacketType_ThreadStart:
				bb_strncpy(t->threadName, decoded->packet.threadStart.text, sizeof(t->threadName));
				Fonts_CacheGlyphs(t->threadName);
				for(viewIndex = 0; viewIndex < session->views.count; ++viewIndex) {
					view_t *view = session->views.data + viewIndex;
					view_set_thread_name(view, t->id, t->threadName);
				}
				recorded_session_sort_threads(session);
				break;
			case kBBPacketType_ThreadName:
				bb_strncpy(t->threadName, decoded->packet.threadName.text, sizeof(t->threadName));
				Fonts_CacheGlyphs(t->threadName);
				for(viewIndex = 0; viewIndex < session->views.count; ++viewIndex) {
					view_t *view = session->views.data + viewIndex;
					view_set_thread_name(view, t->id, t->threadName);
				}
				recorded_session_sort_threads(session);
				break;
			default: break;
			}
			return t;
		}
	}
	t = bba_add(session->threads, 1);
	if(t) {
		t->id = threadId;
		t->startTime = decoded->header.timestamp;
		switch(decoded->type) {
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
		for(i = 0; i < session->views.count; ++i) {
			view_add_thread(session->views.data + i, t);
		}
		recorded_session_sort_threads(session);
	}
	return t;
}

recorded_pieInstance_t *recorded_session_find_pieInstance(recorded_session_t *session, u32 pieInstance)
{
	if(session->pieInstances.count > pieInstance) {
		return session->pieInstances.data + pieInstance;
	}
	return NULL;
}

static recorded_pieInstance_t *recorded_session_find_or_add_pieInstance(recorded_session_t *session, u32 pieInstance)
{
	if(session->pieInstances.count <= pieInstance) {
		bba_add(session->pieInstances, pieInstance - session->pieInstances.count + 1);
		for(u32 viewIndex = 0; viewIndex < session->views.count; ++viewIndex) {
			view_add_pieInstance(session->views.data + viewIndex, pieInstance);
		}
	}
	if(session->pieInstances.count > pieInstance) {
		return session->pieInstances.data + pieInstance;
	}
	return NULL;
}
