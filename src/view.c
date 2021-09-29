// Copyright (c) 2012-2021 Matt Campbell
// MIT license (see License.txt)

#include "view.h"
#include "bb_array.h"
#include "bb_string.h"
#include "bb_structs_generated.h"
#include "imgui_core.h"
#include "line_parser.h"
#include "recorded_session.h"
#include "tags.h"
#include "view_config.h"
#include <sqlite/wrap_sqlite3.h>
#include <stdlib.h>

static void view_add_log_internal(view_t *view, recorded_log_t *log, u32 persistentLogIndex);

extern const char *g_view_column_long_display_names[] = {
	"Line (Absolute)",
	"Line (View-relative)",
	"Date",
	"Time",
	"Elapsed ms (Absolute)",
	"Delta ms (Absolute)",
	"Delta ms (View-relative)",
	"Filename",
	"Thread",
	"PIE Instance",
	"Category",
};
BB_CTASSERT(BB_ARRAYSIZE(g_view_column_long_display_names) == kColumn_Count);

extern const char *g_view_column_display_names[] = {
	"Line",
	"Line",
	"Date",
	"Time",
	"Elapsed ms",
	"Delta ms",
	"Delta ms",
	"Filename",
	"Thread",
	"PIE",
	"Category",
};
BB_CTASSERT(BB_ARRAYSIZE(g_view_column_display_names) == kColumn_Count);

extern const char *g_view_column_config_names[] = {
	"AbsIndex",
	"ViewIndex",
	"Date",
	"Time",
	"AbsMilliseconds",
	"AbsDeltaMilliseconds",
	"ViewDeltaMilliseconds",
	"Filename",
	"Thread",
	"PIEInstance",
	"Category",
};
BB_CTASSERT(BB_ARRAYSIZE(g_view_column_config_names) == kColumn_Count);

static view_column_t s_column_defaults[] = {
	{ true, 60.0f },   // kColumn_AbsIndex,
	{ false, 60.0f },  // kColumn_ViewIndex,
	{ false, 80.0f },  // kColumn_Date,
	{ true, 140.0f },  // kColumn_Time,
	{ false, 110.0f }, // kColumn_AbsMilliseconds,
	{ false, 110.0f }, // kColumn_AbsDeltaMilliseconds,
	{ true, 110.0f },  // kColumn_ViewDeltaMilliseconds,
	{ false, 140.0f }, // kColumn_Filename,
	{ false, 130.0f }, // kColumn_Thread,
	{ false, 40.0f },  // kColumn_PIEInstance,
	{ true, 140.0f },  // kColumn_Category,
};
BB_CTASSERT(BB_ARRAYSIZE(s_column_defaults) == kColumn_Count);

static void view_category_copy(view_category_t *c, recorded_category_t *category)
{
	memcpy(c->categoryName, category->categoryName, sizeof(c->categoryName));
	c->id = category->id;
	c->selected = false;
	c->visible = true;
}

static int ViewCategoryCompare(const void *_a, const void *_b)
{
	const view_category_t *a = (const view_category_t *)_a;
	const view_category_t *b = (const view_category_t *)_b;
	return strcmp(a->categoryName, b->categoryName);
}

const char *view_get_create_table_command(void)
{
	return "CREATE TABLE logs (line INTEGER PRIMARY KEY, category TEXT, level TEXT, pie NUMBER, text TEXT);";
}

void view_init(view_t *view, recorded_session_t *session, b8 autoClose)
{
	u32 i, j;
	view->tiled = true;
	view->session = session;
	view->open = true;
	view->viewId = session->nextViewId++;
	view->tail = true;
	view->visibleLogsDirty = true;
	view->autoClose = autoClose || g_config.autoCloseManual;
	view->prevScrollY = 0.0f;
	view->prevDpiScale = Imgui_Core_GetDpiScale();
	view->bookmarkThreshold = -1;
	view->gotoTargetInput = 0;
	view->gotoTarget = -1;
	view->gotoVisible = false;
	view->categoriesWidth = 200.0f;
	view->lastVisibleSelectedSessionIndexStart = ~0U;
	view->lastVisibleSessionIndexStart = ~0U;
	view->visibleLogs.lastClickIndex = ~0U;
	view->consoleHistory.pos = ~0U;
	view->lastCategoryClickIndex = ~0U;
	view->lastSessionLogIndex = ~0U;
	view->lastVisibleSessionLogIndex = ~0U;
	view->redockCount = 2;

	int rc = sqlite3_open(":memory:", &view->db);
	if(rc != SQLITE_OK) {
		if(view->db) {
			sqlite3_close(view->db);
			view->db = NULL;
		}
	}
	if(view->db) {
		char *errorMessage = NULL;
		rc = sqlite3_exec(view->db, view_get_create_table_command(), NULL, NULL, &errorMessage);
		if(rc != SQLITE_OK) {
			BB_ERROR("sqlite", "SQL error running CREATE TABLE: %s", errorMessage);
			sqlite3_free(errorMessage);
			sqlite3_close(view->db);
			view->db = NULL;
		}
	}

	view->config.showSelectorTarget = false;
	view->config.showVeryVerbose = view->config.showVerbose = false;
	view->config.showLogs = view->config.showDisplay = view->config.showWarnings = view->config.showErrors = view->config.showFatal = true;
	view->config.newNonFavoriteCategoryVisibility = true;
	view->config.newThreadVisibility = true;
	view->config.newFileVisibility = true;

	for(i = 0; i < kColumn_Count; ++i) {
		view_column_t *column = view->columns + i;
		*column = s_column_defaults[i];
		column->width *= Imgui_Core_GetDpiScale();
		column->offset = 0.0f;
	}

	for(i = 0; i < session->categories.count; ++i) {
		view_category_t *c = bba_add(view->categories, 1);
		if(c) {
			view_category_copy(c, session->categories.data + i);
		}
	}
	qsort(view->categories.data, view->categories.count, sizeof(view->categories.data[0]), ViewCategoryCompare);
	for(i = 0; i < session->threads.count; ++i) {
		view_add_thread(view, session->threads.data + i);
	}

	for(i = 0; i < session->filenames.count; ++i) {
		view_add_file(view, session->filenames.data + i);
	}

	if(view->session->appInfo.type != kBBPacketType_Invalid) {
		view_init_appinfo(view);
	}

	for(i = 0; i < session->logs.count; ++i) {
		recorded_log_t *log = session->logs.data[i];
		for(j = 0; j < log->numLines; ++j) {
			view_persistent_log_t *persistent = bba_add(view->persistentLogs, 1);
			persistent->sessionLogIndex = log->sessionLogIndex;
			persistent->subLine = j;
			persistent->bookmarked = false;
		}
	}
	view->visibleLogsAdded = true;

	for(i = 0; i < session->pieInstances.count; ++i) {
		view_add_pieInstance(view, session->pieInstances.data[i].pieInstance);
	}
}

void view_init_appinfo(view_t *view)
{
	BB_LOG("View", "init appinfo: %s\n", view->session->appInfo.packet.appInfo.applicationName);
	view_config_read(view);
}

void view_reset(view_t *view)
{
	if(view_config_write(view)) {
		//BB_LOG("View", "%s wrote config\n", view->session->appInfo.packet.appInfo.applicationName);
	} else {
		BB_ERROR("View", "%s failed to write config\n", view->session->appInfo.packet.appInfo.applicationName);
	}

	bba_free(view->threads);
	bba_free(view->files);
	bba_free(view->categories);
	bba_free(view->pieInstances);
	bba_free(view->visibleLogs);
	bba_free(view->persistentLogs);
	vfilter_reset(&view->vfilter);
	bba_free(view->spans);
	view_config_reset(&view->config);
	view_session_config_reset(&view->sessionConfig);
	view_config_logs_reset(&view->configLogs);
	sb_reset(&view->consoleInput);
	view_console_history_reset(&view->consoleHistory);
	mb_shutdown(&view->messageboxes);
	if(view->db) {
		sqlite3_close(view->db);
		view->db = NULL;
	}
	sb_reset(&view->sqlSelect);
	sb_reset(&view->sqlSelectError);
}

void view_restart(view_t *view)
{
	if(view_config_write(view)) {
		//BB_LOG("View::Config", "%s wrote config\n", view->session->appInfo.packet.appInfo.applicationName);
	} else {
		BB_ERROR("View::Config", "%s failed to write config\n", view->session->appInfo.packet.appInfo.applicationName);
	}
	bba_free(view->threads);
	bba_free(view->files);
	bba_free(view->categories);
	bba_free(view->pieInstances);
	bba_free(view->visibleLogs);
	bba_free(view->persistentLogs);
	bba_free(view->spans);
	view_config_reset(&view->config);
	view_session_config_reset(&view->sessionConfig);
	view_config_logs_reset(&view->configLogs);
}

void view_reset_column_offsets(view_t *view)
{
	int i;
	for(i = 0; i < kColumn_Count; ++i) {
		view_column_t *column = view->columns + i;
		column->offset = 0.0f;
	}
	//BB_LOG("View::Columns", "%s reset column offsets", view->session->appInfo.packet.appInfo.applicationName);
}

void view_reset_column_widths(view_t *view)
{
	int i;
	for(i = 0; i < kColumn_Count; ++i) {
		view_column_t *column = view->columns + i;
		column->width = s_column_defaults[i].width * Imgui_Core_GetDpiScale();
		column->offset = 0.0f;
	}
	//BB_LOG("View::Columns", "%s reset column widths", view->session->appInfo.packet.appInfo.applicationName);
}

void view_add_category(view_t *view, recorded_category_t *category, const view_config_category_t *configCategory)
{
	view_category_t *c = bba_add(view->categories, 1);
	if(c) {
		view_category_copy(c, category);
		if(configCategory) {
			view_apply_config_category(configCategory, c);
			if(view->changedNonFavoriteColumnVisibility) {
				c->visible = view->config.newNonFavoriteCategoryVisibility;
			}
		} else {
			//BB_LOG("Config::Category", "%s skipped apply for missing '%s' during add", view->session->applicationFilename, c->categoryName);
			c->visible = view->config.newNonFavoriteCategoryVisibility;
		}
		qsort(view->categories.data, view->categories.count, sizeof(view->categories.data[0]), ViewCategoryCompare);
		view_apply_tag_visibility(view);
	}
}

static int ViewThreadCompare(const void *_a, const void *_b)
{
	const view_thread_t *a = (const view_thread_t *)_a;
	const view_thread_t *b = (const view_thread_t *)_b;
	int res = strcmp(a->threadName, b->threadName);
	if(res) {
		return res;
	} else {
		return a->id > b->id ? 1 : -1;
	}
}
static void view_sort_threads(view_t *view)
{
	qsort(view->threads.data, view->threads.count, sizeof(view->threads.data[0]), ViewThreadCompare);
}
void view_add_thread(view_t *view, recorded_thread_t *rt)
{
	view_thread_t *t = bba_add(view->threads, 1);
	if(t) {
		//BB_LOG("View::AddThread", "view_add_thread %lu %s\n", rt->id, rt->threadName);
		bb_strncpy(t->threadName, rt->threadName, sizeof(t->threadName));
		t->id = rt->id;
		t->selected = false;
		t->visible = view->config.newThreadVisibility;
		view_set_thread_name(view, rt->id, rt->threadName);
	}
}

void view_set_thread_name(view_t *view, u64 id, const char *name)
{
	u32 i;
	for(i = 0; i < view->threads.count; ++i) {
		view_thread_t *t = view->threads.data + i;
		if(t->id == id) {
			view_config_thread_t *ct;
			bb_strncpy(t->threadName, name, sizeof(t->threadName));
			ct = view_find_config_thread(view, t->threadName);
			//BB_LOG("View::SetThreadName", "view_set_thread_name %lu %s (config:%d)\n", id, t->threadName, (ct) ? 1 : 0);
			if(ct) {
				view_apply_config_thread(ct, t);
			}
			view_sort_threads(view);
			break;
		}
	}
}

void view_set_category_collection_visiblity(view_category_collection_t *categoryCollection, b32 visible)
{
	if(categoryCollection) {
		for(u32 i = 0; i < categoryCollection->viewCategories.count; ++i) {
			view_category_t *category = categoryCollection->viewCategories.data[i];
			if(category->visible != visible) {
				category->visible = visible;
				categoryCollection->view->visibleLogsDirty = true;
			}
		}
		view_apply_tag_visibility(categoryCollection->view);
	}
}

void view_set_category_collection_selection(view_category_collection_t *categoryCollection, b32 selected)
{
	if(categoryCollection) {
		for(u32 i = 0; i < categoryCollection->viewCategories.count; ++i) {
			view_category_t *category = categoryCollection->viewCategories.data[i];
			category->selected = selected;
		}
	}
}

void view_set_category_collection_disabled(view_category_collection_t *categoryCollection, b32 disabled)
{
	if(categoryCollection) {
		for(u32 i = 0; i < categoryCollection->viewCategories.count; ++i) {
			view_category_t *category = categoryCollection->viewCategories.data[i];
			if(category->disabled != disabled) {
				category->disabled = disabled;
				categoryCollection->view->visibleLogsDirty = true;
			}
		}
	}
}

void view_apply_tag_visibility(view_t *view)
{
	for(u32 i = 0; i < g_tags.tags.count; ++i) {
		const tag_t *tag = g_tags.tags.data + i;
		if(tag->visibility == kTagVisibility_AlwaysVisible || tag->visibility == kTagVisibility_AlwaysHidden) {
			for(u32 j = 0; j < tag->categories.count; ++j) {
				const sb_t *name = tag->categories.data + j;
				view_category_t *category = view_find_category_by_name(view, sb_get(name));
				if(category) {
					category->visible = tag->visibility == kTagVisibility_AlwaysVisible;
				}
			}
		}
	}
}

void view_remove_unreferenced_categories(view_category_collection_t *categoryCollection)
{
	if(categoryCollection) {
		for(u32 i = 0; i < categoryCollection->viewCategories.count; ++i) {
			view_category_t *category = categoryCollection->viewCategories.data[i];
			if(category->id == 0) {
				category->removed = true;
			}
		}
	}
}

void view_collect_categories_by_tag(view_t *view, view_category_collection_t *matching, view_category_collection_t *unmatching, tag_t *tag, b32 bIncludeHidden)
{
	if(matching) {
		matching->view = view;
		matching->viewCategories.count = 0;
	}
	if(unmatching) {
		unmatching->view = view;
		unmatching->viewCategories.count = 0;
	}

	for(u32 i = 0; i < view->categories.count; ++i) {
		view_category_t *viewCategory = view->categories.data + i;
		if(!bIncludeHidden && view_category_treat_as_empty(viewCategory)) {
			continue;
		}
		const char *categoryName = viewCategory->categoryName;
		b32 bMatching = (!tag || sbs_contains(&tag->categories, sb_from_c_string_no_alloc(categoryName)));
		if(bMatching && matching) {
			bba_push(matching->viewCategories, viewCategory);
		} else if(!bMatching && unmatching) {
			bba_push(unmatching->viewCategories, viewCategory);
		}
	}
}

b32 view_category_treat_as_empty(view_category_t *viewCategory)
{
	if(viewCategory->removed && viewCategory->id == 0) {
		return true;
	}
	return !g_config.showEmptyCategories && !viewCategory->id;
}

void view_collect_categories_by_selection(view_t *view, view_category_collection_t *matching, view_category_collection_t *unmatching)
{
	if(matching) {
		matching->view = view;
		matching->viewCategories.count = 0;
	}
	if(unmatching) {
		unmatching->view = view;
		unmatching->viewCategories.count = 0;
	}

	for(u32 i = 0; i < view->categories.count; ++i) {
		view_category_t *viewCategory = view->categories.data + i;
		if(view_category_treat_as_empty(viewCategory)) {
			continue;
		}
		b32 bMatching = viewCategory->selected;
		if(bMatching && matching) {
			bba_push(matching->viewCategories, viewCategory);
		} else if(!bMatching && unmatching) {
			bba_push(unmatching->viewCategories, viewCategory);
		}
	}
}

b32 view_category_visible(view_t *view, u32 categoryId)
{
	u32 categoryIndex;
	for(categoryIndex = 0; categoryIndex < view->categories.count; ++categoryIndex) {
		view_category_t *category = view->categories.data + categoryIndex;
		if(category->id == categoryId) {
			return category->visible && !category->disabled;
		}
	}
	return false;
}

b32 view_thread_visible(view_t *view, u64 threadId)
{
	u32 threadIndex;
	for(threadIndex = 0; threadIndex < view->threads.count; ++threadIndex) {
		view_thread_t *t = view->threads.data + threadIndex;
		if(t->id == threadId) {
			return t->visible;
		}
	}
	return false;
}

static const char *ViewGetFilenameFromPath(const char *path)
{
	const char *sep = strrchr(path, '/');
	if(sep)
		return sep + 1;
	sep = strrchr(path, '\\');
	if(sep)
		return sep + 1;
	return path;
}

static int ViewFileCompare(const void *_a, const void *_b)
{
	const view_file_t *a = (const view_file_t *)_a;
	const view_file_t *b = (const view_file_t *)_b;
	const char *fname_a = ViewGetFilenameFromPath(a->path);
	const char *fname_b = ViewGetFilenameFromPath(b->path);
	int res = strcmp(fname_a, fname_b);
	if(res) {
		return res;
	} else {
		return a->id > b->id ? 1 : -1;
	}
}
static void view_sort_files(view_t *view)
{
	qsort(view->files.data, view->files.count, sizeof(view->files.data[0]), ViewFileCompare);
}
void view_add_file(view_t *view, recorded_filename_t *rt)
{
	view_file_t *t = bba_add(view->files, 1);
	if(t) {
		view_config_file_t *cf;
		//BB_LOG("View::Addfile", "view_add_file %lu %s\n", rt->id, rt->path);
		bb_strncpy(t->path, rt->path, sizeof(t->path));
		t->id = rt->id;
		t->selected = false;
		t->visible = view->config.newFileVisibility;

		cf = view_find_config_file(view, t->path);
		//BB_LOG("View::AddFile", "view_add_file %lu %s (config:%d)\n", t->id, t->path, (cf) ? 1 : 0);
		if(cf) {
			view_apply_config_file(cf, t);
		}

		view_sort_files(view);
	}
}

b32 view_file_visible(view_t *view, u64 fileId)
{
	u32 fileIndex;
	for(fileIndex = 0; fileIndex < view->files.count; ++fileIndex) {
		view_file_t *t = view->files.data + fileIndex;
		if(t->id == fileId) {
			return t->visible;
		}
	}
	return false;
}

static int view_pieInstance_sort(const void *_a, const void *_b)
{
	const view_pieInstance_t *a = (const view_pieInstance_t *)_a;
	const view_pieInstance_t *b = (const view_pieInstance_t *)_b;
	return a->pieInstance > b->pieInstance;
}

void view_add_pieInstance(view_t *view, s32 pieInstance)
{
	for(u32 i = 0; i < view->pieInstances.count; ++i) {
		view_pieInstance_t *viewPieInstance = view->pieInstances.data + i;
		if(viewPieInstance->pieInstance == pieInstance) {
			return;
		}
	}

	view_pieInstance_t *viewPieInstance = bba_add(view->pieInstances, 1);
	if(viewPieInstance) {
		viewPieInstance->visible = true;
		viewPieInstance->pieInstance = pieInstance;
		viewPieInstance->primary = view->pieInstances.count == 1;
		qsort(view->pieInstances.data, view->pieInstances.count, sizeof(view->pieInstances.data[0]), view_pieInstance_sort);
	}
}

view_pieInstance_t *view_find_pieInstance(view_t *view, s32 pieInstance)
{
	for(u32 i = 0; i < view->pieInstances.count; ++i) {
		view_pieInstance_t *viewPieInstance = view->pieInstances.data + i;
		if(viewPieInstance->pieInstance == pieInstance) {
			return viewPieInstance;
		}
	}
	return NULL;
}

view_category_t *view_find_category(view_t *view, u32 categoryId)
{
	u32 categoryIndex;
	for(categoryIndex = 0; categoryIndex < view->categories.count; ++categoryIndex) {
		view_category_t *category = view->categories.data + categoryIndex;
		if(category->id == categoryId) {
			return category;
		}
	}
	return NULL;
}

static b32 view_is_log_visible(view_t *view, recorded_log_t *log)
{
	bb_decoded_packet_t *decoded = &log->packet;
	u32 level = decoded->packet.logText.level;
	u32 categoryId;
	u64 threadId;
	u64 fileId;
	s32 pieInstance = decoded->packet.logText.pieInstance;
	b32 levelVisible = false;
	switch(level) {
	case kBBLogLevel_VeryVerbose:
		levelVisible = view->config.showVeryVerbose;
		break;
	case kBBLogLevel_Verbose:
		levelVisible = view->config.showVerbose;
		break;
	case kBBLogLevel_Log:
		levelVisible = view->config.showLogs;
		break;
	case kBBLogLevel_Display:
		levelVisible = view->config.showDisplay;
		break;
	case kBBLogLevel_Warning:
		levelVisible = view->config.showWarnings;
		break;
	case kBBLogLevel_Error:
		levelVisible = view->config.showErrors;
		break;
	case kBBLogLevel_Fatal:
		levelVisible = view->config.showFatal;
		break;
	case kBBLogLevel_SetColor:
		break;
	default:
		BB_ASSERT(false);
		break;
	}
	if(!levelVisible)
		return false;
	view_pieInstance_t *viewPieInstance = view_find_pieInstance(view, pieInstance);
	if(!viewPieInstance || !viewPieInstance->visible)
		return false;
	threadId = decoded->header.threadId;
	if(!view_thread_visible(view, threadId))
		return false;
	categoryId = decoded->packet.logText.categoryId;
	if(!view_category_visible(view, categoryId))
		return false;
	fileId = decoded->header.fileId;
	if(!view_file_visible(view, fileId))
		return false;
	if(!view_filter_visible(view, log))
		return false;
	if(view->spans.count && view->spansActive) {
		u32 sessionLogIndex = log->sessionLogIndex;
		b32 ok = false;
		u32 i;
		for(i = 0; i < view->spans.count; ++i) {
			view_span_t *span = view->spans.data + i;
			if(sessionLogIndex >= span->start && sessionLogIndex <= span->end) {
				ok = true;
				break;
			}
		}
		if(!ok) {
			return false;
		}
	}
	return true;
}

const char *view_get_select_statement_fmt(void)
{
	return "SELECT line FROM logs WHERE %s";
}

static void view_update_sqlWhere(view_t *view)
{
	bba_clear(view->sqlSelect);
	if(view->vfilter.type == kVF_SQL && view->vfilter.tokens.count > 1) {
		u64 skip = span_length(view->vfilter.tokens.data[0].span);
		if(view->vfilter.input.count > skip) {
			sb_va(&view->sqlSelect, view_get_select_statement_fmt(), sb_get(&view->vfilter.input) + skip);
		}
	}
}

static void view_update_spans(view_t *view)
{
	char tokenBuffer[256];
	char *token;
	line_parser_t parser;
	bba_clear(view->spans);
	bb_strncpy(tokenBuffer, sb_get(&view->config.spansInput), sizeof(tokenBuffer));
	line_parser_init(&parser, "spans", tokenBuffer);
	parser.commentCharacter = 0;
	parser.tokenCursor = parser.buffer;
	token = line_parser_next_token(&parser);
	while(token) {
		view_span_t entry;
		char *separator = strchr(token, '-');
		if(!separator) {
			separator = strchr(token, '~');
			if(!separator) {
				separator = strchr(token, '+');
			}
		}
		entry.end = ~0U;
		if(separator) {
			*separator++ = '\0';
			if(*separator) {
				entry.end = (u32)atoi(separator);
			}
		} else {
			entry.end = (u32)atoi(token);
		}
		entry.start = (u32)atoi(token);
		bba_push(view->spans, entry);
		token = line_parser_next_token(&parser);
	}
}

void view_remove_all_bookmarks(view_t *view)
{
	u32 i;
	for(i = 0; i < view->persistentLogs.count; ++i) {
		view_persistent_log_t *log = view->persistentLogs.data + i;
		log->bookmarked = false;
	}
	for(i = 0; i < view->visibleLogs.count; ++i) {
		view_log_t *log = view->visibleLogs.data + i;
		log->bookmarked = false;
	}
}

void view_toggle_bookmarks_for_selection(view_t *view)
{
	u32 i;
	b32 bookmarked = true;
	for(i = 0; i < view->visibleLogs.count; ++i) {
		view_log_t *log = view->visibleLogs.data + i;
		if(log->selected) {
			bookmarked = bookmarked && log->bookmarked;
		}
	}

	for(i = 0; i < view->visibleLogs.count; ++i) {
		view_log_t *log = view->visibleLogs.data + i;
		if(log->selected) {
			view_persistent_log_t *persistent_log = view->persistentLogs.data + log->persistentLogIndex;
			persistent_log->bookmarked = !bookmarked;
			log->bookmarked = !bookmarked;
		}
	}
}

void view_advance_to_next_bookmark(view_t *view, b32 forward)
{
	int i;
	int best = -1;
	int first = -1;
	int last = -1;
	for(i = 0; i < (int)view->visibleLogs.count; ++i) {
		view_log_t *log = view->visibleLogs.data + i;
		if(log->bookmarked) {
			if(first < 0) {
				first = i;
			}
			last = i;
			if(forward) {
				if(i > view->bookmarkThreshold && best < 0) {
					best = i;
				}
			} else {
				if(i < view->bookmarkThreshold) {
					best = i;
				}
			}
		}
	}
	if(best >= 0) {
		view->gotoTarget = best;
	} else if(forward) {
		if(first >= 0) {
			view->gotoTarget = first;
		}
	} else {
		if(last >= 0) {
			view->gotoTarget = last;
		}
	}
	if(view->gotoTarget >= 0) {
		view->bookmarkThreshold = view->gotoTarget;
	}
}

view_category_t *view_get_log_category(view_t *view, view_log_t *viewLog)
{
	u32 sessionLogIndex = viewLog->sessionLogIndex;
	recorded_session_t *session = view->session;
	recorded_log_t *sessionLog = session->logs.data[sessionLogIndex];
	bb_decoded_packet_t *decoded = &sessionLog->packet;
	recorded_category_t *recordedCategory = recorded_session_find_category(session, decoded->packet.logText.categoryId);
	view_category_t *viewCategory = view_find_category_by_name(view, recordedCategory->categoryName);
	return viewCategory;
}

u32 view_get_log_category_index(view_t *view, view_log_t *viewLog)
{
	view_category_t *viewCategory = view_get_log_category(view, viewLog);
	return (u32)(viewCategory - view->categories.data);
}

void view_update_visible_logs(view_t *view)
{
	u32 i, j;
	u32 persistentLogIndex = 0;
	recorded_session_t *session = view->session;
	view_logs_t oldLogs = view->visibleLogs;
	u32 lastClickIndex = view->visibleLogs.lastClickIndex;
	memset(&view->visibleLogs, 0, sizeof(view->visibleLogs));
	view->visibleLogs.lastClickIndex = lastClickIndex;
	view->lastSessionLogIndex = ~0U;
	view->lastVisibleSessionLogIndex = ~0U;
	vfilter_reset(&view->vfilter);
	view->vfilter = view_filter_parse(sb_get(&view->config.filterInput));
	view_update_sqlWhere(view);
	view_update_spans(view);
	for(i = 0; i < session->logs.count; ++i) {
		recorded_log_t *log = session->logs.data[i];
		if(view_is_log_visible(view, log)) {
			for(j = persistentLogIndex; j < view->persistentLogs.count; ++j) {
				view_persistent_log_t *persistent = view->persistentLogs.data + j;
				if(log->sessionLogIndex == persistent->sessionLogIndex) {
					persistentLogIndex = j;
					break;
				}
			}
			view_add_log_internal(view, log, persistentLogIndex);
			view->lastVisibleSessionLogIndex = i;
		}
		view->lastSessionLogIndex = i;
	}
	i = j = 0;
	while(i < oldLogs.count && j < view->visibleLogs.count) {
		view_log_t *a = oldLogs.data + i;
		view_log_t *b = view->visibleLogs.data + j;
		if(a->sessionLogIndex < b->sessionLogIndex) {
			++i;
		} else if(a->sessionLogIndex == b->sessionLogIndex) {
			if(a->subLine < b->subLine) {
				++i;
			} else if(a->subLine == b->subLine) {
				b->selected = a->selected;
				++i;
				++j;
			} else {
				++j;
			}
		} else {
			++j;
		}
	}
	bba_free(oldLogs);
}

void view_update_category_id(view_t *view, recorded_category_t *category)
{
	for(u32 i = 0; i < view->categories.count; ++i) {
		view_category_t *c = view->categories.data + i;
		if(!strcmp(c->categoryName, category->categoryName)) {
			c->id = category->id;
		}
	}
}

view_category_t *view_find_category_by_name(view_t *view, const char *categoryName)
{
	for(u32 i = 0; i < view->categories.count; ++i) {
		view_category_t *c = view->categories.data + i;
		if(!strcmp(c->categoryName, categoryName)) {
			return c;
		}
	}
	return NULL;
}

void view_set_all_category_visibility(view_t *view, b8 visible)
{
	u32 i;
	BB_LOG("Debug", "%s all categories for '%s'\n",
	       visible ? "Checked" : "Unchecked",
	       view->session->appInfo.packet.appInfo.applicationName);
	view->visibleLogsDirty = true;
	view->config.newNonFavoriteCategoryVisibility = visible;
	for(i = 0; i < view->categories.count; ++i) {
		view_category_t *c = view->categories.data + i;
		c->visible = visible;
	}
	view_apply_tag_visibility(view);
}

void view_set_all_thread_visibility(view_t *view, b8 visible)
{
	u32 i;
	BB_LOG("Debug", "%s all thread for '%s'\n",
	       visible ? "Checked" : "Unchecked",
	       view->session->appInfo.packet.appInfo.applicationName);
	view->visibleLogsDirty = true;
	view->config.newThreadVisibility = visible;
	for(i = 0; i < view->threads.count; ++i) {
		view_thread_t *t = view->threads.data + i;
		t->visible = visible;
	}
	for(i = 0; i < view->config.configThreads.count; ++i) {
		view_config_thread_t *t = view->config.configThreads.data + i;
		t->visible = visible;
	}
}

void view_set_all_file_visibility(view_t *view, b8 visible)
{
	u32 i;
	BB_LOG("Debug", "%s all file for '%s'\n",
	       visible ? "Checked" : "Unchecked",
	       view->session->appInfo.packet.appInfo.applicationName);
	view->visibleLogsDirty = true;
	view->config.newFileVisibility = visible;
	for(i = 0; i < view->files.count; ++i) {
		view_file_t *t = view->files.data + i;
		t->visible = visible;
	}
	for(i = 0; i < view->config.configFiles.count; ++i) {
		view_config_file_t *t = view->config.configFiles.data + i;
		t->visible = visible;
	}
}

void view_set_all_pieinstance_visibility(view_t *view, b8 visible)
{
	u32 i;
	BB_LOG("Debug", "%s all pieInstances for '%s'\n",
	       visible ? "Checked" : "Unchecked",
	       view->session->appInfo.packet.appInfo.applicationName);
	view->visibleLogsDirty = true;
	for(i = 0; i < view->pieInstances.count; ++i) {
		view_pieInstance_t *t = view->pieInstances.data + i;
		t->visible = visible;
	}
}

static b32 view_get_config_log_bookmarked(view_t *view, u32 sessionLogIndex, u32 subLine)
{
	while(view->nextConfigBookmarkedLogIndex < view->sessionConfig.bookmarkedLogs.count) {
		view_config_log_index_t *logIndex = view->sessionConfig.bookmarkedLogs.data + view->nextConfigBookmarkedLogIndex;
		if(logIndex->sessionLogIndex > sessionLogIndex)
			break;

		if(logIndex->sessionLogIndex == sessionLogIndex) {
			if(logIndex->subLine > subLine)
				break;

			if(logIndex->subLine == subLine) {
				return true;
			}
		}

		++view->nextConfigBookmarkedLogIndex;
	}
	return false;
}

static b32 view_get_config_log_selected(view_t *view, u32 sessionLogIndex, u32 subLine)
{
	while(view->nextConfigSelectedLogIndex < view->sessionConfig.selectedLogs.count) {
		view_config_log_index_t *logIndex = view->sessionConfig.selectedLogs.data + view->nextConfigSelectedLogIndex;
		if(logIndex->sessionLogIndex > sessionLogIndex)
			break;

		if(logIndex->sessionLogIndex == sessionLogIndex) {
			if(logIndex->subLine > subLine)
				break;

			if(logIndex->subLine == subLine) {
				return true;
			}
		}

		++view->nextConfigSelectedLogIndex;
	}
	return false;
}

void view_add_log(view_t *view, recorded_log_t *log)
{
	u32 i;
	u32 persistentLogIndex = view->persistentLogs.count;
	for(i = 0; i < log->numLines; ++i) {
		view_persistent_log_t *persistent = bba_add(view->persistentLogs, 1);
		persistent->sessionLogIndex = log->sessionLogIndex;
		persistent->subLine = i;
		persistent->bookmarked = view_get_config_log_bookmarked(view, persistent->sessionLogIndex, persistent->subLine);
	}
	u32 visibleLogCount = view->visibleLogs.count;
	view_add_log_internal(view, log, persistentLogIndex);
	view->lastSessionLogIndex = log->sessionLogIndex;
	if(visibleLogCount < view->visibleLogs.count) {
		view->visibleLogsAdded = true;
		view->lastVisibleSessionLogIndex = log->sessionLogIndex;

		for(i = visibleLogCount; i < view->visibleLogs.count; ++i) {
			view_log_t *visibleLog = view->visibleLogs.data + i;
			visibleLog->selected = view_get_config_log_selected(view, visibleLog->sessionLogIndex, visibleLog->subLine);
		}
	}
}

static void view_add_log_internal(view_t *view, recorded_log_t *log, u32 persistentLogIndex)
{
	view_persistent_log_t *persistent = view->persistentLogs.data + persistentLogIndex;
	if(view_is_log_visible(view, log)) {
		view_log_t *visibleLog = bba_add(view->visibleLogs, 1);
		if(visibleLog) {
			visibleLog->sessionLogIndex = log->sessionLogIndex;
			if(view->visibleLogs.count == 1) {
				visibleLog->viewLogIndex = 0;
			} else {
				visibleLog->viewLogIndex = view->visibleLogs.data[view->visibleLogs.count - 2].viewLogIndex + 1;
			}
			//visibleLog->viewLogIndex = view->visibleLogs.count - 1;
			visibleLog->selected = false;
			visibleLog->subLine = 0;
			visibleLog->persistentLogIndex = persistentLogIndex;
			visibleLog->bookmarked = persistent->bookmarked;
			if(log->numLines > 1) {
				view_log_t baseLog = *visibleLog; // the sub-lines' bba_add_noclear can reallocate and invalidate visibleLog
				u32 i;
				for(i = 1; i < log->numLines; ++i) {
					view_log_t *subLineLog = bba_add_noclear(view->visibleLogs, 1);
					if(subLineLog) {
						persistent = view->persistentLogs.data + persistentLogIndex + i;
						*subLineLog = baseLog;
						subLineLog->subLine = i;
						subLineLog->persistentLogIndex = persistentLogIndex + i;
						subLineLog->bookmarked = persistent->bookmarked;
						//subLineLog->viewLogIndex = view->visibleLogs.count - 1;
					}
				}
			}
		}
	}
}
