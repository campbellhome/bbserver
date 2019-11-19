// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "config.h"

#include "bb_criticalsection.h"
#include "bb_packet.h"
#include "bb_thread.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct span_s span_t;
typedef struct view_s view_t;

typedef struct session_message_queue_s {
	volatile s64 readCursor;
	volatile s64 writeCursor;
	bb_critical_section cs; // #TODO: single producer, single consumer shouldn't lock - just use InterlockedIncrement, InterlockedCompare
	bb_decoded_packet_t entries[512];
} session_message_queue_t;

typedef struct views_s {
	u32 count;
	u32 allocated;
	view_t *data;
} views_t;

typedef struct recorded_log_s {
	u32 sessionLogIndex;
	u32 numLines;
	bb_decoded_packet_t packet;
} recorded_log_t;
typedef struct recorded_logs_s {
	u32 count;
	u32 allocated;
	recorded_log_t **data;
} recorded_logs_t;

typedef struct partial_logs_s {
	u32 count;
	u32 allocated;
	bb_decoded_packet_t *data;
} partial_logs_t;

typedef struct recorded_category_s {
	char categoryName[kBBSize_Category];
	u32 id;
	u32 depth;
	u32 parentIndex;
	u32 logCount[kBBLogLevel_Count];
	u32 logCountIncludingChildren[kBBLogLevel_Count];
} recorded_category_t;

typedef struct recorded_categories_s {
	u32 count;
	u32 allocated;
	recorded_category_t *data;
} recorded_categories_t;

typedef struct recorded_filename_s {
	char path[kBBSize_MaxPath];
	u32 id;
	u32 logCount[kBBLogLevel_Count];
} recorded_filename_t;

typedef struct recorded_filenames_s {
	u32 count;
	u32 allocated;
	recorded_filename_t *data;
} recorded_filenames_t;

typedef struct recorded_thread_s {
	char threadName[kBBSize_ThreadName];
	u64 id;
	u64 startTime;
	u64 endTime;
	u32 logCount[kBBLogLevel_Count];
} recorded_thread_t;

typedef struct recorded_threads_s {
	u32 count;
	u32 allocated;
	recorded_thread_t *data;
} recorded_threads_t;

typedef struct recorded_pieInstance_s {
	u32 logCount[kBBLogLevel_Count];
} recorded_pieInstance_t;

typedef struct recorded_pieInstances_s {
	u32 count;
	u32 allocated;
	recorded_pieInstance_t *data;
} recorded_pieInstances_t;

typedef struct recorded_session_s {
	u8 recvBuffer[32768];
	u8 terminator;
	char path[kBBSize_MaxPath];
	char applicationFilename[kBBSize_ApplicationName];
	u8 threadDesiredActive;
	u8 threadActive;
	b8 logReads;
	b8 recordingActive;
	b8 failedToDeserialize;
	b8 shownDeserializationMessageBox;
	u8 pad;
	bb_decoded_packet_t appInfo;
	views_t views;
	recorded_logs_t logs;
	partial_logs_t partialLogs;
	recorded_categories_t categories;
	recorded_filenames_t filenames;
	recorded_threads_t threads;
	recorded_pieInstances_t pieInstances;
	bb_thread_handle_t threadHandle;
	u32 nextViewId;
	u32 outgoingMqId;
	session_message_queue_t *incoming;
} recorded_session_t;

void recorded_session_open(const char *path, const char *applicationFilename, b8 autoClose, b32 recordingActive, u32 outgoingMqId);
void recorded_session_close(recorded_session_t *session);
void recorded_session_update(recorded_session_t *session);
void recorded_session_auto_close(const char *applicationName);
void recorded_session_auto_close_all(void);
void recorded_session_recording_stopped(const char *path);
void recorded_session_add_config_category(recorded_session_t *session, const char *categoryName);
recorded_session_t *recorded_session_find(const char *path);
u32 recorded_session_count(void);
recorded_session_t *recorded_session_get(u32 index);
recorded_category_t *recorded_session_find_category_by_name(recorded_session_t *session, const char *categoryName);
recorded_category_t *recorded_session_find_category(recorded_session_t *session, u32 categoryId);
recorded_filename_t *recorded_session_find_filename(recorded_session_t *session, u32 fileId);
recorded_thread_t *recorded_session_find_thread(recorded_session_t *session, u64 threadId);
recorded_pieInstance_t *recorded_session_find_pieInstance(recorded_session_t *session, u32 pieInstance);

#if defined(__cplusplus)
}
#endif
