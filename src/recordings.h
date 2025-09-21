// Copyright (c) Matt Campbell
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

AUTOJSON typedef enum recording_tab_t {
	kRecordingTab_Internal,
	kRecordingTab_External,
	kRecordingTab_Count
} recording_tab_t;

AUTOJSON typedef enum recording_sort_e {
	kRecordingSort_StartTime,
	kRecordingSort_Application,
	kRecordingSort_Count
} recording_sort_t;

AUTOJSON typedef enum recording_group_e {
	kRecordingGroup_None,
	kRecordingGroup_Application,
	kRecordingGroup_Count
} recording_group_t;

AUTOJSON typedef enum recording_type_e {
	kRecordingType_Normal,
	kRecordingType_ExistingFile,
	kRecordingType_MainLog,
	kRecordingType_ExternalFile,
	kRecordingType_Count
} recording_type_t;

AUTOSTRUCT typedef struct recording_s
{
	char applicationName[kBBSize_ApplicationName];
	char applicationFilename[kBBSize_ApplicationName];
	char path[kBBSize_MaxPath];
	u32 id;
	u32 filetimeHigh;
	u32 filetimeLow;
	b32 active;
	recording_type_t recordingType;
	u32 outgoingMqId;
	u32 platform;
	u32 pendingDelete;
	u8 pad[4];
} recording_t;

AUTOJSON typedef struct new_recording_s
{
	sb_t applicationName;
	sb_t applicationFilename;
	sb_t path;
	FILETIME filetime;
	b32 openView;
	recording_type_t recordingType;
	u32 mqId;
	u32 platform;
} new_recording_t;

typedef struct recordingIds_s
{
	u32 count;
	u32 allocated;
	u32* data;
} recordingIds_t;

AUTOJSON typedef struct recordings_tab_config_s
{
	recording_group_t group;
	recording_sort_t sort;
	b32 showDate;
	b32 showTime;
	b32 showInternal;
	b32 showExternal;
} recordings_tab_config_t;

AUTOJSON typedef struct recordings_config_s
{
	recordings_tab_config_t tabs[kRecordingTab_Count];
	float width;
	b32 recordingsOpen;
} recordings_config_t;

AUTOSTRUCT typedef struct recordings_s
{
	u32 count;
	u32 allocated;
	recording_t* data;
} recordings_t;

AUTOSTRUCT typedef struct grouped_recording_entry_s
{
	recording_t* recording;
	u32 groupId;
	b32 selected;
} grouped_recording_entry_t;

AUTOSTRUCT typedef struct grouped_recordings_s
{
	u32 count;
	u32 allocated;
	u32 lastClickIndex;
	u8 pad[4];
	grouped_recording_entry_t* data;
} grouped_recordings_t;

AUTOSTRUCT typedef struct recordings_tab_data_s
{
	recordings_t allRecordings;
	recordings_t invalidRecordings;
	grouped_recordings_t groupedRecordings;
	b32 dirty;
	u8 pad[4];
} recordings_tab_data_t;

void recordings_init(void);
void recordings_shutdown(void);
b32 recordings_are_dirty(recording_tab_t tab);
void recordings_clear_dirty(recording_tab_t tab);
void recordings_sort(recording_tab_t tab);
void recordings_autodelete_old_recordings(void);
recordings_t* recordings_get_all(recording_tab_t tab);
recordings_config_t* recordings_get_config(void);
grouped_recordings_t* grouped_recordings_get_all(recording_tab_t tab);

recording_t* recordings_find_by_id(u32 id);
recording_t* recordings_find_by_path(const char* path);
recording_t* recordings_find_main_log(void);
const char* recording_build_start_identifier(new_recording_t recording);
void recording_add_existing(char* data, b32 valid);
void recording_started(char* data);
void recording_stopped(char* data);
b32 recordings_get_application_info(const char* path, bb_decoded_packet_t* decoded);
void recordings_validate_max_recordings(void);

b32 recordings_delete_by_id(u32 id);

#if defined(__cplusplus)
}
#endif
