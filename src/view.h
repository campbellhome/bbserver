// Copyright (c) 2012-2021 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "config.h"
#include "message_box.h"
#include "view_filter.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct recorded_session_s recorded_session_t;
typedef struct recorded_category_s recorded_category_t;
typedef struct recorded_log_s recorded_log_t;
typedef struct recorded_thread_s recorded_thread_t;
typedef struct recorded_filename_s recorded_filename_t;
typedef struct bb_decoded_packet_s bb_decoded_packet_t;
typedef struct sqlite3 sqlite3;
typedef struct tag_s tag_t;
typedef struct view_s view_t;

typedef struct view_category_s {
	char categoryName[kBBSize_Category];
	u32 id;
	b32 selected;
	b32 visible;
	b32 disabled;
	b32 removed;
	u8 pad[4];
} view_category_t;
typedef struct view_categories_s {
	u32 count;
	u32 allocated;
	view_category_t *data;
} view_categories_t;

typedef struct view_thread_s {
	char threadName[kBBSize_ThreadName];
	u64 id;
	b32 selected;
	b32 visible;
} view_thread_t;
typedef struct view_threads_s {
	u32 count;
	u32 allocated;
	view_thread_t *data;
} view_threads_t;

typedef struct view_file_s {
	char path[kBBSize_MaxPath];
	u32 id;
	b32 selected;
	b32 visible;
	u8 pad[4];
} view_file_t;
typedef struct view_files_s {
	u32 count;
	u32 allocated;
	view_file_t *data;
} view_files_t;

AUTOJSON typedef struct view_pieInstance_s {
	b32 selected;
	b32 visible;
	s32 pieInstance;
	b32 primary;
} view_pieInstance_t;
AUTOJSON typedef struct view_pieInstances_s {
	u32 count;
	u32 allocated;
	view_pieInstance_t *data;
} view_pieInstances_t;

typedef struct view_log_s {
	u32 sessionLogIndex;
	u32 viewLogIndex;
	u32 persistentLogIndex;
	u32 subLine;
	b32 selected;
	b32 bookmarked;
	u8 pad[4];
} view_log_t;

typedef struct view_logs_s {
	u32 count;
	u32 allocated;
	u32 lastClickIndex;
	u8 pad[4];
	view_log_t *data;
} view_logs_t;

typedef struct view_persistent_log_s {
	u32 sessionLogIndex;
	u32 subLine;
	b32 bookmarked;
	u8 pad[4];
} view_persistent_log_t;

typedef struct view_persistent_logs_s {
	u32 count;
	u32 allocated;
	view_persistent_log_t *data;
} view_persistent_logs_t;

AUTOJSON typedef struct view_config_thread_s {
	sb_t name;
	b32 selected;
	b32 visible;
} view_config_thread_t;
AUTOJSON typedef struct view_config_threads_s {
	u32 count;
	u32 allocated;
	view_config_thread_t *data;
} view_config_threads_t;

AUTOJSON typedef struct view_config_file_s {
	sb_t path;
	b32 selected;
	b32 visible;
} view_config_file_t;
AUTOJSON typedef struct view_config_files_s {
	u32 count;
	u32 allocated;
	view_config_file_t *data;
} view_config_files_t;

AUTOJSON typedef struct view_config_category_s {
	sb_t name;
	b32 selected;
	b32 visible;
	u32 depth;
	b32 disabled;
} view_config_category_t;
AUTOJSON typedef struct view_config_categories_s {
	u32 count;
	u32 allocated;
	view_config_category_t *data;
} view_config_categories_t;

typedef struct collected_view_categories_s {
	u32 count;
	u32 allocated;
	view_category_t **data;
} collected_view_categories_t;
typedef struct view_category_collection_s {
	view_t *view;
	collected_view_categories_t viewCategories;
} view_category_collection_t;

AUTOJSON typedef struct view_config_column_s {
	sb_t name;
	b32 visible;
	float width;
} view_config_column_t;

AUTOJSON typedef struct view_config_columns_s {
	u32 count;
	u32 allocated;
	view_config_column_t *data;
} view_config_columns_t;

AUTOJSON typedef struct view_console_history_entry_t {
	sb_t command;
} view_console_history_entry_t;

AUTOJSON typedef struct view_console_entries_t {
	u32 count;
	u32 allocated;
	view_console_history_entry_t *data;
} view_console_history_entries_t;

AUTOJSON typedef struct view_console_history_t {
	view_console_history_entries_t entries;
	u32 pos;
	u8 pad[4];
} view_console_history_t;

AUTOJSON typedef struct view_config_s {
	view_config_columns_t configColumns;
	view_config_threads_t configThreads;
	view_config_files_t configFiles;
	view_config_categories_t configCategories;
	view_console_history_t consoleHistory;
	sb_t filterInput;
	sb_t spansInput;
	b32 showVeryVerbose;
	b32 showVerbose;
	b32 showLogs;
	b32 showDisplay;
	b32 showWarnings;
	b32 showErrors;
	b32 showFatal;
	b32 showSelectorTarget;
	b32 newNonFavoriteCategoryVisibility;
	b32 newThreadVisibility;
	b32 newFileVisibility;
	b32 filterActive;
	b32 showFilterHelp;
	u32 version;
} view_config_t;

enum { kViewConfigVersion = 2 };

AUTOJSON typedef struct view_config_log_s {
	u32 sessionLogIndex;
	u32 subLine;
	b32 bookmarked;
	b32 selected;
} view_config_log_t;
AUTOJSON typedef struct view_config_logs_s {
	u32 count;
	u32 allocated;
	view_config_log_t *data;
} view_config_logs_t;

AUTOJSON typedef struct view_config_log_index_s {
	u32 sessionLogIndex;
	u32 subLine;
} view_config_log_index_t;
AUTOJSON typedef struct view_config_log_indices_s {
	u32 count;
	u32 allocated;
	view_config_log_index_t *data;
} view_config_log_indices_t;

AUTOJSON typedef struct view_session_config_s {
	view_config_t viewConfig;
	view_config_log_indices_t selectedLogs;
	view_config_log_indices_t bookmarkedLogs;
	u32 version;
	u32 pad;
} view_session_config_t;

enum { kViewSessionConfigVersion = 1 };

typedef struct view_span_s {
	u32 start;
	u32 end;
} view_span_t;
typedef struct view_spans_s {
	u32 count;
	u32 allocated;
	view_span_t *data;
} view_spans_t;

typedef enum {
	kColumn_AbsIndex,
	kColumn_ViewIndex,
	kColumn_Date,
	kColumn_Time,
	kColumn_AbsMilliseconds,
	kColumn_AbsDeltaMilliseconds,
	kColumn_ViewDeltaMilliseconds,
	kColumn_Filename,
	kColumn_Thread,
	kColumn_PIEInstance,
	kColumn_Category,
	kColumn_Count
} view_column_e;
extern const char *g_view_column_long_display_names[];
extern const char *g_view_column_display_names[];
extern const char *g_view_column_config_names[];

typedef struct view_column_s {
	b32 visible;
	float width;
	float offset;
} view_column_t;

typedef struct view_s {
	view_config_t config;
	view_session_config_t sessionConfig;
	view_config_logs_t configLogs;
	recorded_session_t *session;
	sqlite3 *db;
	sb_t sqlSelect;
	sb_t sqlSelectError;
	messageBoxes messageboxes;
	view_threads_t threads;
	view_files_t files;
	view_categories_t categories;
	view_pieInstances_t pieInstances;
	view_logs_t visibleLogs;
	view_persistent_logs_t persistentLogs;
	vfilter_t vfilter;
	view_spans_t spans;
	sb_t consoleInput;
	view_console_history_t consoleHistory;
	view_column_t columns[kColumn_Count];
	b32 open;
	b32 initialized;
	u32 viewId;
	u32 nextConfigBookmarkedLogIndex;
	u32 nextConfigSelectedLogIndex;
	u32 lastSessionLogIndex;
	u32 lastVisibleSessionLogIndex;
	u32 lastVisibleSessionIndexStart;
	u32 lastVisibleSessionIndexEnd;
	u32 lastVisibleSelectedSessionIndexStart;
	u32 lastVisibleSelectedSessionIndexEnd;
	u32 lastCategoryClickIndex;
	float categoriesWidth;
	float combinedColumnsWidth;
	float scrollWidth;
	float prevScrollX;
	float prevScrollY;
	float prevDpiScale;
	int bookmarkThreshold;
	int gotoTargetInput;
	int gotoTarget;
	b8 gotoVisible;
	b8 gotoViewRelative;
	b8 visibleLogsDirty;
	b8 tail;
	b8 autoClose;
	b8 consoleInputFocused;
	b8 consoleRequestActive;
	b8 consoleInputActive;
	b8 spansActive;
	b8 changedNonFavoriteColumnVisibility;
	b8 tiled;
	b8 beingDragged;
	b8 visibleLogsAdded;
	b8 externalView;
	s8 redockCount;
	u8 pad[1];
} view_t;

void view_init(view_t *view, recorded_session_t *session, b8 autoClose);
void view_init_appinfo(view_t *view);
const char *view_get_create_table_command(void);
const char *view_get_select_statement_fmt(void);
void view_reset(view_t *view);
void view_restart(view_t *view);
void view_reset_column_offsets(view_t *view);
void view_reset_column_widths(view_t *view);
void view_add_category(view_t *view, recorded_category_t *category, const view_config_category_t *configCategory);
void view_add_thread(view_t *view, recorded_thread_t *rt);
void view_add_file(view_t *view, recorded_filename_t *rf);
void view_add_pieInstance(view_t *view, s32 pieInstance);
view_pieInstance_t *view_find_pieInstance(view_t *view, s32 pieInstance);
void view_add_log(view_t *view, recorded_log_t *log);
void view_update_visible_logs(view_t *view);
void view_update_category_id(view_t *view, recorded_category_t *category);
void view_set_thread_name(view_t *view, u64 id, const char *name);
view_category_t *view_find_category(view_t *view, u32 categoryId);
view_category_t *view_find_category_by_name(view_t *view, const char *categoryName);
view_category_t *view_get_log_category(view_t *view, view_log_t *viewLog);
u32 view_get_log_category_index(view_t *view, view_log_t *viewLog);
b32 view_category_treat_as_empty(view_category_t *viewCategory);
void view_collect_categories_by_tag(view_t *view, view_category_collection_t *matching, view_category_collection_t *unmatching, tag_t *tag, b32 bIncludeHidden);
void view_collect_categories_by_selection(view_t *view, view_category_collection_t *matching, view_category_collection_t *unmatching);
void view_set_category_collection_visiblity(view_category_collection_t *categoryCollection, b32 visible);
void view_set_category_collection_selection(view_category_collection_t *categoryCollection, b32 selected);
void view_set_category_collection_disabled(view_category_collection_t *categoryCollection, b32 disabled);
void view_apply_tag_visibility(view_t *view);
void view_remove_unreferenced_categories(view_category_collection_t *categoryCollection);
void view_set_all_category_visibility(view_t *view, b8 visible);
void view_set_all_thread_visibility(view_t *view, b8 visible);
void view_set_all_file_visibility(view_t *view, b8 visible);
void view_set_all_pieinstance_visibility(view_t *view, b8 visible);
void view_remove_all_bookmarks(view_t *view);
void view_toggle_bookmarks_for_selection(view_t *view);
void view_advance_to_next_bookmark(view_t *view, b32 forward);

#if defined(__cplusplus)
}
#endif
