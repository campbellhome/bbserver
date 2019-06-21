// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "config.h"

#if defined(__cplusplus)
extern "C" {
#endif

// define the Windows structs for preproc
#if 0
AUTOJSON typedef struct _FILETIME {
    unsigned long dwLowDateTime;
    unsigned long dwHighDateTime;
} FILETIME;
#endif

typedef enum recording_sort_e {
	kRecordingSort_StartTime,
	kRecordingSort_Application,
	kRecordingSort_Count
} recording_sort_t;

typedef enum recording_group_e {
	kRecordingGroup_None,
	kRecordingGroup_Application,
	kRecordingGroup_Count
} recording_group_t;

typedef struct recording_s {
	char applicationName[kBBSize_ApplicationName];
	char applicationFilename[kBBSize_ApplicationName];
	char path[kBBSize_MaxPath];
	u32 id;
	u32 filetimeHigh;
	u32 filetimeLow;
	b32 active;
	b32 mainLog;
	u32 outgoingMqId;
	u32 platform;
} recording_t;

AUTOJSON typedef struct new_recording_s {
	sb_t applicationName;
	sb_t applicationFilename;
	sb_t path;
	FILETIME filetime;
	b32 openView;
	b32 mainLog;
	u32 mqId;
	u32 platform;
} new_recording_t;

typedef struct recordingIds_s {
	u32 count;
	u32 allocated;
	u32 *data;
} recordingIds_t;

typedef struct recordings_s {
	u32 count;
	u32 allocated;
	recording_t *data;
	recording_group_t group;
	recording_sort_t sort;
	f32 width;
	b32 showDate;
	b32 showTime;
	u8 pad[4];
} recordings_t;

typedef struct grouped_recording_entry_s {
	recording_t *recording;
	u32 groupId;
	b32 selected;
} grouped_recording_entry_t;

typedef struct grouped_recordings_s {
	u32 count;
	u32 allocated;
	u32 lastClickIndex;
	u8 pad[4];
	grouped_recording_entry_t *data;
} grouped_recordings_t;

void recordings_init(void);
void recordings_shutdown(void);
b32 recordings_are_dirty(void);
void recordings_clear_dirty(void);
void recordings_sort(void);
void recordings_autodelete_old_recordings(void);
recordings_t *recordings_get_all(void);
grouped_recordings_t *grouped_recordings_get_all(void);

recording_t *recordings_find_by_id(u32 id);
recording_t *recordings_find_by_path(const char *path);
recording_t *recordings_find_main_log(void);
const char *recording_build_start_identifier(new_recording_t recording);
void recording_add_existing(char *data, b32 valid);
void recording_started(char *data);
void recording_stopped(char *data);

b32 recordings_delete_by_id(u32 id, recordings_t *recordings);

#if defined(__cplusplus)
}
#endif
