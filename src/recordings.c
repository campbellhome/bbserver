// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "recordings.h"
#include "bb_json_generated.h"
#include "bb_structs_generated.h"
#include "config.h"
#include "fonts.h"
#include "message_queue.h"
#include "recorded_session.h"
#include "recordings_config.h"
#include "sb.h"
#include "va.h"

#include "bb_thread.h"
#include "bbclient/bb_array.h"
#include "bbclient/bb_packet.h"
#include "bbclient/bb_string.h"

#include "bbclient/bb_wrap_stdio.h"
#include "globals.h"
#include "parson/parson.h"
#include <stdlib.h>

void sanitize_app_filename(const char *applicationName, char *applicationFilename, size_t applicationFilenameLen);

static recordings_t s_recordings;
static recordings_t s_invalidRecordings;
static grouped_recordings_t s_groupedRecordings;
static u32 s_nextRecordingId;
static b32 s_recordingsDirty;

recording_t *recordings_find_by_id(u32 id)
{
	u32 i;
	for(i = 0; i < s_recordings.count; ++i) {
		recording_t *r = s_recordings.data + i;
		if(r->id == id)
			return r;
	}
	return NULL;
}

recording_t *recordings_find_by_path(const char *path)
{
	u32 i;
	for(i = 0; i < s_recordings.count; ++i) {
		recording_t *r = s_recordings.data + i;
		if(!strcmp(r->path, path))
			return r;
	}
	return NULL;
}

recording_t *recordings_find_main_log(void)
{
	u32 i;
	for(i = 0; i < s_recordings.count; ++i) {
		recording_t *r = s_recordings.data + i;
		if(r->mainLog != 0)
			return r;
	}
	return NULL;
}

static char *recording_get_next_string(char *src)
{
	char *str = (src) ? strchr(src, '\n') : NULL;
	if(!str)
		return NULL;
	*str = '\0';
	return str + 1;
}

const char *recording_build_start_identifier(new_recording_t recording)
{
	char *result = "";
	JSON_Value *json = json_serialize_new_recording_t(&recording);
	if(json) {
		char *str = json_serialize_to_string(json);
		if(str) {
			result = va("%s", str);
			json_free_serialized_string(str);
		}
		json_value_free(json);
	}
	return result;
}

static new_recording_t recording_build_new_recording(char *data)
{
	new_recording_t r = { BB_EMPTY_INITIALIZER };
	r.mqId = mq_invalid_id();
	JSON_Value *json = json_parse_string(data);
	if(json) {
		r = json_deserialize_new_recording_t(json);
		json_value_free(json);
	}
	return r;
}

static int recordings_compare_starttime(const void *_a, const void *_b)
{
	const recording_t *a = (const recording_t *)_a;
	const recording_t *b = (const recording_t *)_b;
	ULARGE_INTEGER atime;
	ULARGE_INTEGER btime;
	atime.LowPart = a->filetimeLow;
	atime.HighPart = a->filetimeHigh;
	btime.LowPart = b->filetimeLow;
	btime.HighPart = b->filetimeHigh;
	if(atime.QuadPart != btime.QuadPart) {
		return (int)(atime.QuadPart > btime.QuadPart) ? 1 : -1;
	}
	return strcmp(a->path, b->path);
}
static int recordings_compare_application(const void *_a, const void *_b)
{
	const recording_t *a = (const recording_t *)_a;
	const recording_t *b = (const recording_t *)_b;
	int applicationCompare = strcmp(a->applicationName, b->applicationName);
	if(applicationCompare) {
		return applicationCompare;
	}
	if(a->filetimeHigh != b->filetimeHigh) {
		return a->filetimeHigh - b->filetimeHigh;
	}
	if(a->filetimeLow != b->filetimeLow) {
		return a->filetimeLow - b->filetimeLow;
	}
	return strcmp(a->path, b->path);
}
static void recordings_add_group(void)
{
	grouped_recording_entry_t *prev = s_groupedRecordings.count ? s_groupedRecordings.data + s_groupedRecordings.count - 1 : NULL;
	grouped_recording_entry_t *g = bba_add(s_groupedRecordings, 1);
	if(g) {
		g->groupId = (prev) ? prev->groupId + 1 : 1;
	}
}
static void recordings_add_grouped_recording(recording_t *r)
{
	grouped_recording_entry_t *prev = s_groupedRecordings.data + s_groupedRecordings.count - 1;
	grouped_recording_entry_t *g = bba_add(s_groupedRecordings, 1);
	if(g) {
		g->groupId = prev->groupId;
		g->recording = r;
	}
}
void recordings_sort(void)
{
	u32 i;
	bba_clear(s_groupedRecordings);
	s_groupedRecordings.lastClickIndex = ~0u;
	switch(s_recordings.sort) {
	case kRecordingSort_StartTime:
		qsort(s_recordings.data, s_recordings.count, sizeof(s_recordings.data[0]), recordings_compare_starttime);
		break;
	case kRecordingSort_Application:
		qsort(s_recordings.data, s_recordings.count, sizeof(s_recordings.data[0]), recordings_compare_application);
		break;
	default:
		BB_ASSERT(0);
		break;
	}

	switch(s_recordings.group) {
	case kRecordingGroup_None:
		for(i = 0; i < s_recordings.count; ++i) {
			grouped_recording_entry_t *g = bba_add(s_groupedRecordings, 1);
			recording_t *r = s_recordings.data + i;
			if(g) {
				g->recording = r;
				g->groupId = i;
			}
		}
		break;
	case kRecordingGroup_Application:
		for(i = 0; i < s_recordings.count; ++i) {
			recording_t *r = s_recordings.data + i;
			if(i) {
				recording_t *prev = s_recordings.data + i - 1;
				if(strcmp(prev->applicationName, r->applicationName)) {
					recordings_add_group();
				}
				recordings_add_grouped_recording(r);
			} else {
				recordings_add_group();
				recordings_add_grouped_recording(r);
			}
		}
		break;
	default:
		BB_ASSERT(0);
	}
}

void recording_add_existing(char *data, b32 valid)
{
	recording_t *recording;
	new_recording_t r = recording_build_new_recording(data);
	if(sb_len(&r.path)) {
		if(!recordings_find_by_path(sb_get(&r.path))) {
			recordings_t *recordings = (valid) ? &s_recordings : &s_invalidRecordings;
			recording = bba_add(*recordings, 1);
			if(recording) {
				recording->id = ++s_nextRecordingId;
				recording->active = false;
				bb_strncpy(recording->applicationName, sb_get(&r.applicationName), sizeof(recording->applicationName));
				bb_strncpy(recording->applicationFilename, sb_get(&r.applicationFilename), sizeof(recording->applicationFilename));
				bb_strncpy(recording->path, sb_get(&r.path), sizeof(recording->path));
				Fonts_CacheGlyphs(recording->applicationName);
				Fonts_CacheGlyphs(recording->applicationFilename);
				Fonts_CacheGlyphs(recording->path);
				recording->filetimeHigh = r.filetime.dwHighDateTime;
				recording->filetimeLow = r.filetime.dwLowDateTime;
				recording->outgoingMqId = mq_invalid_id();
				recording->platform = r.platform;
				s_recordingsDirty = true;
			}
		}
	}
	new_recording_reset(&r);
}

void recording_started(char *data)
{
	recording_t *recording, *existing;
	new_recording_t r = recording_build_new_recording(data);
	if(sb_len(&r.path)) {
		existing = recordings_find_by_path(sb_get(&r.path));
		if(existing) {
			existing->active = true;
			// #todo: existing sessions should become inactive and not try to read from the file any more
		} else {
			recording = bba_add(s_recordings, 1);
			if(recording) {
				recording->id = ++s_nextRecordingId;
				recording->active = true;
				recording->mainLog = r.mainLog;
				bb_strncpy(recording->applicationName, sb_get(&r.applicationName), sizeof(recording->applicationName));
				bb_strncpy(recording->applicationFilename, sb_get(&r.applicationFilename), sizeof(recording->applicationFilename));
				bb_strncpy(recording->path, sb_get(&r.path), sizeof(recording->path));
				Fonts_CacheGlyphs(recording->applicationName);
				Fonts_CacheGlyphs(recording->applicationFilename);
				Fonts_CacheGlyphs(recording->path);
				recording->filetimeHigh = r.filetime.dwHighDateTime;
				recording->filetimeLow = r.filetime.dwLowDateTime;
				recording->platform = r.platform;
				if(r.mqId == mq_invalid_id()) {
					recording->outgoingMqId = mq_invalid_id();
				} else {
					recording->outgoingMqId = mq_addref(r.mqId);
				}
				s_recordingsDirty = true;
				if(r.openView) {
					if(g_config.autoCloseAll) {
						recorded_session_auto_close_all();
					} else {
						recorded_session_auto_close(sb_get(&r.applicationName));
					}
					recorded_session_open(sb_get(&r.path), sb_get(&r.applicationFilename), true, true, recording->outgoingMqId);
				}
			}
		}
	}
	new_recording_reset(&r);
}

void recording_stopped(char *data)
{
	u32 i;
	char *path = strchr(data, '\n');
	if(!path)
		return;
	++path;
	for(i = 0; i < s_recordings.count; ++i) {
		recording_t *recording = s_recordings.data + i;
		if(!strcmp(recording->path, path)) {
			recording->active = false;
			mq_releaseref(recording->outgoingMqId);
			recorded_session_recording_stopped(recording->path);
		}
	}
}

b32 recordings_delete_by_id(u32 id, recordings_t *recordings)
{
	u32 i;
	if(!recordings) {
		recordings = &s_recordings;
	}
	for(i = 0; i < recordings->count; ++i) {
		recording_t *r = recordings->data + i;
		if(r->id == id) {
			char *ext;
			sb_t logPath;
			const char *path = recordings->data[i].path;
			BOOL ret = DeleteFileA(path);
			if(ret) {
				BB_LOG("Recordings", "Deleted '%s'", path);
			} else {
				BB_ERROR("Recordings", "Failed to delete '%s' - errno is %d", path, GetLastError());
			}
			sb_init(&logPath);
			sb_append(&logPath, path);
			ext = strrchr(logPath.data, '.');
			if(ext) {
				logPath.count = (u32)(ext + 1 - logPath.data);
			}
			sb_append(&logPath, ".log");
			if(logPath.count > 1) {
				ret = DeleteFileA(logPath.data);
				if(ret) {
					BB_LOG("Recordings", "Deleted '%s'", logPath.data);
				} else {
					DWORD err = GetLastError();
					if(err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND) {
						BB_ERROR("Recordings", "Failed to delete '%s' - errno is %d", logPath.data, err);
					}
				}
			}
			sb_reset(&logPath);
			bba_erase(*recordings, i);
			return true;
		}
	}
	return false;
}

static void recordings_check_autodelete(ULARGE_INTEGER nowInt, recordingIds_t *pendingDeletions, recording_t *recording)
{
	if(!recording->active) {
		ULARGE_INTEGER fileInt;
		fileInt.LowPart = recording->filetimeLow;
		fileInt.HighPart = recording->filetimeHigh;
		if(fileInt.QuadPart < nowInt.QuadPart) {
			// filetimes are in 100 nanosecond intervals - 0.0000001
			ULONGLONG delta = nowInt.QuadPart - fileInt.QuadPart;
			float seconds = delta * 0.0000001f;
			float minutes = seconds / 60;
			float hours = minutes / 60;
			float days = hours / 24;
			if(days > g_config.autoDeleteAfterDays) {
				b32 hasView = recorded_session_find(recording->path) != NULL;
				if(!hasView) {
					BB_LOG("Recordings::AutoDelete", "Deleting %s that is %.0f days old", recording->applicationFilename, days);
					bba_push(*pendingDeletions, recording->id);
				}
			}
		}
	}
}

void recordings_autodelete_old_recordings(void)
{
	if(g_config.autoDeleteAfterDays > 0) {
		u32 i;
		recordingIds_t pendingDeletions;
		recordings_t *recordings = recordings_get_all();
		FILETIME now;
		ULARGE_INTEGER nowInt;
		GetSystemTimeAsFileTime(&now);
		nowInt.LowPart = now.dwLowDateTime;
		nowInt.HighPart = now.dwHighDateTime;

		memset(&pendingDeletions, 0, sizeof(pendingDeletions));

		for(i = 0; i < recordings->count; ++i) {
			recording_t *recording = recordings->data + i;
			recordings_check_autodelete(nowInt, &pendingDeletions, recording);
		}

		for(i = 0; i < s_invalidRecordings.count; ++i) {
			recording_t *recording = s_invalidRecordings.data + i;
			recordings_check_autodelete(nowInt, &pendingDeletions, recording);
		}

		if(pendingDeletions.count) {
			s_recordingsDirty = true;
			for(i = 0; i < pendingDeletions.count; ++i) {
				if(!recordings_delete_by_id(pendingDeletions.data[i], &s_recordings)) {
					recordings_delete_by_id(pendingDeletions.data[i], &s_invalidRecordings);
				}
			}
			bba_free(pendingDeletions);
		}
	}
}

static b32 recordings_get_application_info(const char *path, bb_decoded_packet_t *decoded)
{
	FILE *fp = fopen(path, "rb");
	if(fp) {
		u8 buffer[BB_MAX_PACKET_BUFFER_SIZE];
		size_t nDecodableBytes = fread(buffer, 1, sizeof(buffer), fp);
		u16 nPacketBytes = (nDecodableBytes >= 3) ? (*buffer << 8) + (*(buffer + 1)) : 0;
		if(nPacketBytes == 0 || nPacketBytes > nDecodableBytes) {
			fclose(fp);
			return false;
		}
		fclose(fp);
		if(!bbpacket_deserialize(buffer + 2, nPacketBytes - 2, decoded))
			return false;
		return bbpacket_is_app_info_type(decoded->type);
	} else {
		return false;
	}
}

static void recordings_find_files_in_dir(const char *dir)
{
	bb_decoded_packet_t decoded;
	WIN32_FIND_DATA find;
	HANDLE hFind;

	char filter[1024];
	if(_snprintf(filter, sizeof(filter), "%s\\*.*", dir) < 0) {
		filter[sizeof(filter) - 1] = '\0';
	}
	if(INVALID_HANDLE_VALUE != (hFind = FindFirstFileA(filter, &find))) {
		do {
			if(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if(find.cFileName[0] != '.') {
					if(bb_snprintf(filter, sizeof(filter), "%s\\%s", dir, find.cFileName) < 0) {
						filter[sizeof(filter) - 1] = '\0';
					}
					recordings_find_files_in_dir(filter);
				}
			} else {
				const char *ext = strrchr(find.cFileName, '.');
				if(ext && !_stricmp(ext, ".bbox")) {
					if(bb_snprintf(filter, sizeof(filter), "%s\\%s", dir, find.cFileName) < 0) {
						filter[sizeof(filter) - 1] = '\0';
					}
					memset(&decoded, 0, sizeof(decoded));
					b32 valid = recordings_get_application_info(filter, &decoded);
					char applicationFilename[kBBSize_ApplicationName];
					new_recording_t recording;
					recording.applicationName = sb_from_c_string(decoded.packet.appInfo.applicationName);
					sanitize_app_filename(sb_get(&recording.applicationName), applicationFilename, sizeof(applicationFilename));
					recording.applicationFilename = sb_from_c_string(applicationFilename);
					recording.path = sb_from_c_string(filter);
					recording.filetime = find.ftLastWriteTime;
					recording.openView = false;
					recording.mainLog = false;
					recording.mqId = mq_invalid_id();
					recording.platform = decoded.packet.appInfo.platform;
					to_ui(valid ? kToUI_AddExistingFile : kToUI_AddInvalidExistingFile, "%s", recording_build_start_identifier(recording));
					new_recording_reset(&recording);
				}
			}
		} while(FindNextFileA(hFind, &find));
		FindClose(hFind);
	}
}

void get_appdata_folder(char *buffer, size_t bufferSize);
static bb_thread_return_t recordings_init_thread_func(void *args)
{
	char basePath[1024];
	BB_UNUSED(args);
	bbthread_set_name("recordings_init_thread_func");

	get_appdata_folder(basePath, sizeof(basePath));
	recordings_find_files_in_dir(basePath);
	for(u32 i = 0; i < 32; ++i) {
		to_ui(kToUI_AddExistingFile, "");
	}

	bb_thread_exit(0);
}

static bb_thread_handle_t recordings_thread_id;

void recordings_init(void)
{
	recordings_thread_id = bbthread_create(recordings_init_thread_func, NULL);
	recordings_read_config(&s_recordings);
}

void recordings_shutdown(void)
{
	if(!globals.viewer) {
		recordings_write_config(&s_recordings);
	}
	bba_free(s_recordings);
	bba_free(s_invalidRecordings);
	bba_free(s_groupedRecordings);
	if(recordings_thread_id != 0) {
		bbthread_join(recordings_thread_id);
		recordings_thread_id = 0;
	}
}

b32 recordings_are_dirty(void)
{
	return s_recordingsDirty;
}

void recordings_clear_dirty(void)
{
	s_recordingsDirty = false;
}

recordings_t *recordings_get_all(void)
{
	return &s_recordings;
}

grouped_recordings_t *grouped_recordings_get_all(void)
{
	return &s_groupedRecordings;
}
