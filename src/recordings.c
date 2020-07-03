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

#include "bb_array.h"
#include "bb_packet.h"
#include "bb_string.h"
#include "bb_thread.h"

#include "bb_wrap_stdio.h"
#include "parson/parson.h"
#include <stdlib.h>

#include <ShlObj.h>

void sanitize_app_filename(const char *applicationName, char *applicationFilename, size_t applicationFilenameLen);

static recordings_t s_recordings[kRecordingTab_Count];
static recordings_t s_invalidRecordings[kRecordingTab_Count];
static grouped_recordings_t s_groupedRecordings[kRecordingTab_Count];
static u32 s_nextRecordingId;
static b32 s_recordingsDirty[kRecordingTab_Count];
static recordings_config_t s_recordingsConfig;

static recording_tab_t recording_tab_from_recording_type(recording_type_t recordingType)
{
	return recordingType == kRecordingType_ExternalFile ? kRecordingTab_External : kRecordingTab_Internal;
}

recording_t *recordings_find_by_id(u32 id)
{
	for(u32 tab = 0; tab < kRecordingTab_Count; ++tab) {
		for(u32 i = 0; i < s_recordings[tab].count; ++i) {
			recording_t *r = s_recordings[tab].data + i;
			if(r->id == id)
				return r;
		}
	}
	return NULL;
}

recording_t *recordings_find_by_path(const char *path)
{
	for(u32 tab = 0; tab < kRecordingTab_Count; ++tab) {
		for(u32 i = 0; i < s_recordings[tab].count; ++i) {
			recording_t *r = s_recordings[tab].data + i;
			if(!strcmp(r->path, path))
				return r;
		}
	}
	return NULL;
}

recording_t *recordings_find_main_log(void)
{
	u32 i;
	for(i = 0; i < s_recordings[kRecordingTab_Internal].count; ++i) {
		recording_t *r = s_recordings[kRecordingTab_Internal].data + i;
		if(r->recordingType == kRecordingType_MainLog)
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
static void recordings_add_group(recording_tab_t tab)
{
	grouped_recording_entry_t *prev = s_groupedRecordings[tab].count ? s_groupedRecordings[tab].data + s_groupedRecordings[tab].count - 1 : NULL;
	grouped_recording_entry_t *g = bba_add(s_groupedRecordings[tab], 1);
	if(g) {
		g->groupId = (prev) ? prev->groupId + 1 : 1;
	}
}
static void recordings_add_grouped_recording(recording_tab_t tab, recording_t *r)
{
	grouped_recording_entry_t *prev = s_groupedRecordings[tab].data + s_groupedRecordings[tab].count - 1;
	grouped_recording_entry_t *g = bba_add(s_groupedRecordings[tab], 1);
	if(g) {
		g->groupId = prev->groupId;
		g->recording = r;
	}
}
void recordings_sort(recording_tab_t tab)
{
	u32 i;
	bba_clear(s_groupedRecordings[tab]);
	s_groupedRecordings[tab].lastClickIndex = ~0u;
	switch(s_recordingsConfig.tabs[tab].sort) {
	case kRecordingSort_StartTime:
		qsort(s_recordings[tab].data, s_recordings[tab].count, sizeof(s_recordings[tab].data[0]), recordings_compare_starttime);
		break;
	case kRecordingSort_Application:
		qsort(s_recordings[tab].data, s_recordings[tab].count, sizeof(s_recordings[tab].data[0]), recordings_compare_application);
		break;
	case kRecordingSort_Count:
	default:
		BB_ASSERT(0);
		break;
	}

	b32 bFirst = true;
	switch(s_recordingsConfig.tabs[tab].group) {
	case kRecordingGroup_None:
		for(i = 0; i < s_recordings[tab].count; ++i) {
			recording_t *r = s_recordings[tab].data + i;
			grouped_recording_entry_t *g = bba_add(s_groupedRecordings[tab], 1);
			if(g) {
				g->recording = r;
				g->groupId = i;
			}
		}
		break;
	case kRecordingGroup_Application:
		for(i = 0; i < s_recordings[tab].count; ++i) {
			recording_t *r = s_recordings[tab].data + i;
			if(bFirst) {
				recordings_add_group(tab);
				recordings_add_grouped_recording(tab, r);
				bFirst = false;
			} else {
				recording_t *prev = s_recordings[tab].data + i - 1;
				if(strcmp(prev->applicationName, r->applicationName)) {
					recordings_add_group(tab);
				}
				recordings_add_grouped_recording(tab, r);
			}
		}
		break;
	case kRecordingSort_Count:
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
			recording_tab_t tab = recording_tab_from_recording_type(r.recordingType);
			recordings_t *recordings = (valid) ? s_recordings + tab : s_invalidRecordings + tab;
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
				recording->recordingType = r.recordingType;
				s_recordingsDirty[tab] = true;
			}
		}
	}
	new_recording_reset(&r);
}

void recording_started(char *data)
{
	BB_LOG("Recordings", "%s", data);
	recording_t *recording, *existing;
	new_recording_t r = recording_build_new_recording(data);
	if(sb_len(&r.path)) {
		recording_tab_t tab = recording_tab_from_recording_type(r.recordingType);
		existing = recordings_find_by_path(sb_get(&r.path));
		if(existing) {
			recording = existing;
			tab = recording_tab_from_recording_type(recording->recordingType);
		} else {
			recording = bba_add(s_recordings[tab], 1);
			if(recording) {
				recording->id = ++s_nextRecordingId;
				bb_strncpy(recording->applicationName, sb_get(&r.applicationName), sizeof(recording->applicationName));
				bb_strncpy(recording->applicationFilename, sb_get(&r.applicationFilename), sizeof(recording->applicationFilename));
				bb_strncpy(recording->path, sb_get(&r.path), sizeof(recording->path));
				Fonts_CacheGlyphs(recording->applicationName);
				Fonts_CacheGlyphs(recording->applicationFilename);
				Fonts_CacheGlyphs(recording->path);
				recording->platform = r.platform;
				recording->recordingType = r.recordingType;
				if(r.mqId == mq_invalid_id()) {
					recording->outgoingMqId = mq_invalid_id();
				} else {
					recording->outgoingMqId = mq_addref(r.mqId);
				}
			}
		}

		if(recording) {
			recording->active = r.recordingType == kRecordingType_Normal || r.recordingType == kRecordingType_MainLog;
			recording->recordingType = r.recordingType;
			recording->filetimeHigh = r.filetime.dwHighDateTime;
			recording->filetimeLow = r.filetime.dwLowDateTime;
			s_recordingsDirty[tab] = true;
			if(r.openView) {
				recorded_session_open(sb_get(&r.path), sb_get(&r.applicationFilename), recording->recordingType != kRecordingType_ExternalFile, true, recording->outgoingMqId);
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
	for(u32 tab = 0; tab < kRecordingTab_Count; ++tab) {
		for(i = 0; i < s_recordings[tab].count; ++i) {
			recording_t *recording = s_recordings[tab].data + i;
			if(!strcmp(recording->path, path)) {
				recording->active = false;
				mq_releaseref(recording->outgoingMqId);
				recorded_session_recording_stopped(recording->path);
			}
		}
	}
}

static b32 recordings_delete_by_id_internal(u32 id, recordings_t *recordings)
{
	u32 i;
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

b32 recordings_delete_by_id(u32 id)
{
	for(u32 tab = 0; tab < kRecordingTab_Count; ++tab) {
		if(recordings_delete_by_id_internal(id, s_recordings + tab)) {
			return true;
		}
		if(recordings_delete_by_id_internal(id, s_invalidRecordings + tab)) {
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
		for(u32 tab = 0; tab < kRecordingTab_Count; ++tab) {
			u32 i;
			recordingIds_t pendingDeletions;
			recordings_t *recordings = recordings_get_all(tab);
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

			for(i = 0; i < s_invalidRecordings[tab].count; ++i) {
				recording_t *recording = s_invalidRecordings[tab].data + i;
				recordings_check_autodelete(nowInt, &pendingDeletions, recording);
			}

			if(pendingDeletions.count) {
				s_recordingsDirty[tab] = true;
				for(i = 0; i < pendingDeletions.count; ++i) {
					recordings_delete_by_id(pendingDeletions.data[i]);
				}
				bba_free(pendingDeletions);
			}
		}
	}
}

b32 recordings_get_application_info(const char *path, bb_decoded_packet_t *decoded)
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

static void recordings_find_files_in_dir(const char *dir, b32 bExternal)
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
					recordings_find_files_in_dir(filter, bExternal);
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
					recording.filetime = CompareFileTime(&find.ftLastWriteTime, &find.ftCreationTime) >= 0 ? find.ftLastWriteTime : find.ftCreationTime;
					recording.openView = false;
					recording.recordingType = bExternal ? kRecordingType_ExternalFile : kRecordingType_ExistingFile;
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

static b32 get_downloads_folder(char *buffer, size_t bufferSize)
{
	PWSTR wpath;
	if(SHGetKnownFolderPath(&FOLDERID_Downloads, 0, NULL, &wpath) != S_OK)
		return false;

	buffer[0] = 0;
	size_t numCharsConverted = 0;
	wcstombs_s(&numCharsConverted, buffer, bufferSize, wpath, _TRUNCATE);
	return true;
}

void get_appdata_folder(char *buffer, size_t bufferSize);
static bb_thread_return_t recordings_init_thread_func(void *args)
{
	char basePath[1024];
	BB_UNUSED(args);
	bbthread_set_name("recordings_init_thread_func");

	get_appdata_folder(basePath, sizeof(basePath));
	recordings_find_files_in_dir(basePath, false);

	if(get_downloads_folder(basePath, sizeof(basePath))) {
		recordings_find_files_in_dir(basePath, true);
	}

	bb_thread_exit(0);
}

static bb_thread_handle_t recordings_thread_id;

void recordings_init(void)
{
	recordings_thread_id = bbthread_create(recordings_init_thread_func, NULL);
	recordings_config_read(&s_recordingsConfig);
	s_recordingsConfig.width = 275.0f;
}

void recordings_shutdown(void)
{
	recordings_config_write(&s_recordingsConfig);
	for(u32 tab = 0; tab < kRecordingTab_Count; ++tab) {
		bba_free(s_recordings[tab]);
		bba_free(s_invalidRecordings[tab]);
		bba_free(s_groupedRecordings[tab]);
	}
	if(recordings_thread_id != 0) {
		bbthread_join(recordings_thread_id);
		recordings_thread_id = 0;
	}
}

recordings_config_t *recordings_get_config(void)
{
	return &s_recordingsConfig;
}

b32 recordings_are_dirty(recording_tab_t tab)
{
	return s_recordingsDirty[tab];
}

void recordings_clear_dirty(recording_tab_t tab)
{
	s_recordingsDirty[tab] = false;
}

recordings_t *recordings_get_all(recording_tab_t tab)
{
	return &s_recordings[tab];
}

grouped_recordings_t *grouped_recordings_get_all(recording_tab_t tab)
{
	return &s_groupedRecordings[tab];
}
