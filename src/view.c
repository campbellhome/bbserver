// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "view.h"
#include "bb_array.h"
#include "bb_string.h"
#include "bb_structs_generated.h"
#include "imgui_core.h"
#include "line_parser.h"
#include "recorded_session.h"
#include "view_config.h"
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
	c->depth = category->depth;
	c->selected = false;
	c->visible = true;
	c->favorite = false;
}

static int ViewCategoryCompare(const void *_a, const void *_b)
{
	const view_category_t *a = (const view_category_t *)_a;
	const view_category_t *b = (const view_category_t *)_b;
	return strcmp(a->categoryName, b->categoryName);
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

	view->config.showSelectorTarget = false;
	view->config.showVeryVerbose = view->config.showVerbose = false;
	view->config.showLogs = view->config.showDisplay = view->config.showWarnings = view->config.showErrors = view->config.showFatal = true;
	view->config.newNonFavoriteColumnVisibility = true;
	view->config.newFavoriteColumnVisibility = true;
	view->config.newThreadVisibility = true;
	view->config.newFileVisibility = true;

	for(i = 0; i < kColumn_Count; ++i) {
		view_column_t *column = view->columns + i;
		*column = s_column_defaults[i];
		column->width *= g_config.dpiScale;
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

	for(i = 0; i <= session->pieInstances.count; ++i) {
		view_add_pieInstance(view, i);
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
	bba_free(view->filter);
	bba_free(view->spans);
	view_config_reset(&view->config);
	sb_reset(&view->consoleInput);
	view_console_history_reset(&view->consoleHistory);
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
		column->width = s_column_defaults[i].width * g_config.dpiScale;
		column->offset = 0.0f;
	}
	//BB_LOG("View::Columns", "%s reset column widths", view->session->appInfo.packet.appInfo.applicationName);
}

void view_add_category(view_t *view, recorded_category_t *category)
{
	view_category_t *c = bba_add(view->categories, 1);
	if(c) {
		view_config_category_t *cc;
		view_category_copy(c, category);
		cc = view_find_config_category(view, category->categoryName);
		if(cc) {
			view_apply_config_category(view, cc, c);
			if(c->favorite && view->changedFavoriteColumnVisibility) {
				c->visible = view->config.newFavoriteColumnVisibility;
			} else if(!c->favorite && view->changedNonFavoriteColumnVisibility) {
				c->visible = view->config.newNonFavoriteColumnVisibility;
			}
		} else {
			//BB_LOG("Config::Category", "%s skipped apply for missing '%s' during add", view->session->applicationFilename, c->categoryName);
			c->visible = view->config.newNonFavoriteColumnVisibility;
		}
		qsort(view->categories.data, view->categories.count, sizeof(view->categories.data[0]), ViewCategoryCompare);
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

b32 view_category_visible(view_t *view, u32 categoryId)
{
	u32 categoryIndex;
	for(categoryIndex = 0; categoryIndex < view->categories.count; ++categoryIndex) {
		view_category_t *category = view->categories.data + categoryIndex;
		if(category->id == categoryId) {
			return category->visible;
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

void view_add_pieInstance(view_t *view, u32 pieInstance)
{
	if(view->pieInstances.count <= pieInstance) {
		u32 start = view->pieInstances.count;
		bba_add(view->pieInstances, pieInstance - view->pieInstances.count + 1);
		for(u32 i = start; i < view->pieInstances.count; ++i) {
			view->pieInstances.data[i].visible = true;
		}
	}
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
	u32 pieInstance = decoded->packet.logText.pieInstance;
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
	if(pieInstance >= view->pieInstances.count || !view->pieInstances.data[pieInstance].visible)
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
	if(view->filter.count && view->config.filterActive) {
		const char *text = decoded->packet.logText.text;
		b32 ok = false;
		u32 numRequired = 0;
		u32 numProhibited = 0;
		u32 numAllowed = 0;
		u32 i;
		for(i = 0; i < view->filter.count; ++i) {
			view_filter_token_t *token = view->filter.data + i;
			const char *s = view->filter.tokenBuffer + token->offset;
			b32 required = *s == '+';
			b32 prohibited = *s == '-';
			b32 found;
			numRequired += required;
			numProhibited += prohibited;
			numAllowed += (!required && !prohibited);
			if(required || prohibited) {
				++s;
			}
			found = bb_stristr(text, s) != NULL;
			if(found && prohibited || !found && required) {
				return false;
				break;
			} else if(found) {
				ok = true;
			}
		}
		if(!ok && numAllowed) {
			return false;
		}
	}
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

static void view_update_filter(view_t *view)
{
	char *token;
	line_parser_t parser;
	bba_clear(view->filter);
	bb_strncpy(view->filter.tokenBuffer, sb_get(&view->config.filterInput), sizeof(view->filter.tokenBuffer));
	line_parser_init(&parser, "filter", view->filter.tokenBuffer);
	parser.commentCharacter = 0;
	parser.tokenCursor = parser.buffer;
	token = line_parser_next_token(&parser);
	while(token) {
		view_filter_token_t entry;
		entry.offset = (u32)(token - view->filter.tokenBuffer);
		bba_push(view->filter, entry);
		token = line_parser_next_token(&parser);
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

void view_update_visible_logs(view_t *view)
{
	u32 i, j;
	u32 persistentLogIndex = 0;
	recorded_session_t *session = view->session;
	view_logs_t oldLogs = view->visibleLogs;
	u32 lastClickIndex = view->visibleLogs.lastClickIndex;
	memset(&view->visibleLogs, 0, sizeof(view->visibleLogs));
	view->visibleLogs.lastClickIndex = lastClickIndex;
	view_update_filter(view);
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
		}
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

void view_set_all_category_visibility(view_t *view, b8 visible)
{
	u32 i;
	BB_LOG("Debug", "%s all categories for '%s'\n",
	       visible ? "Checked" : "Unchecked",
	       view->session->appInfo.packet.appInfo.applicationName);
	view->visibleLogsDirty = true;
	view->config.newNonFavoriteColumnVisibility = visible;
	view->config.newFavoriteColumnVisibility = visible;
	for(i = 0; i < view->categories.count; ++i) {
		view_category_t *c = view->categories.data + i;
		c->visible = visible;
	}
	for(i = 0; i < view->config.configCategories.count; ++i) {
		view_config_category_t *c = view->config.configCategories.data + i;
		c->visible = visible;
	}
}

static u32 view_set_favorite_category_visibility_recursive(view_t *view, u32 startIndex, b32 favorites, b8 visible, u32 inheritedFavoriteCount)
{
	recorded_categories_t *categories = &view->session->categories;
	view_categories_t *viewCategories = &view->categories;

	recorded_category_t *category = categories->data + startIndex;
	view_category_t *viewCategory = viewCategories->data + startIndex;

	u32 favoriteCount = inheritedFavoriteCount + viewCategory->favorite ? 1u : 0u;

	u32 endIndex = startIndex + 1;
	while(endIndex < categories->count) {
		recorded_category_t *subCategory = categories->data + endIndex;
		if(subCategory->depth <= category->depth)
			break;
		view_category_t *viewSubCategory = viewCategories->data + endIndex;
		favoriteCount += viewSubCategory->favorite;
		++endIndex;
	}

	if(favorites && !favoriteCount)
		return endIndex;

	if(!favorites && viewCategory->favorite)
		return endIndex;

	viewCategory->visible = visible;

	if(startIndex + 1 != endIndex) {
		u32 index = startIndex + 1;
		while(index < endIndex) {
			index = view_set_favorite_category_visibility_recursive(view, index, favorites, visible, favoriteCount);
		}
	}

	return endIndex;
}

u32 view_test_favorite_category_visibility_recursive(view_t *view, u32 startIndex, b32 favorites, u32 *visibleCount, u32 *hiddenCount, u32 inheritedFavoriteCount)
{
	recorded_categories_t *categories = &view->session->categories;
	view_categories_t *viewCategories = &view->categories;

	recorded_category_t *category = categories->data + startIndex;
	view_category_t *viewCategory = viewCategories->data + startIndex;

	u32 favoriteCount = inheritedFavoriteCount + viewCategory->favorite ? 1u : 0u;

	u32 endIndex = startIndex + 1;
	while(endIndex < categories->count) {
		recorded_category_t *subCategory = categories->data + endIndex;
		if(subCategory->depth <= category->depth)
			break;
		view_category_t *viewSubCategory = viewCategories->data + endIndex;
		favoriteCount += viewSubCategory->favorite;
		++endIndex;
	}

	if(favorites && !favoriteCount)
		return endIndex;

	if(!favorites && viewCategory->favorite)
		return endIndex;

	if(viewCategory->visible && visibleCount) {
		++*visibleCount;
	}

	if(!viewCategory->visible && hiddenCount) {
		++*hiddenCount;
	}

	if(startIndex + 1 != endIndex) {
		u32 index = startIndex + 1;
		while(index < endIndex) {
			index = view_test_favorite_category_visibility_recursive(view, index, favorites, visibleCount, hiddenCount, favoriteCount);
		}
	}

	return endIndex;
}

static u32 view_set_favorite_config_category_visibility(view_t *view, u32 startIndex, b32 favorites, b8 visible, u32 inheritedFavoriteCount)
{
	view_config_categories_t *categories = &view->config.configCategories;
	view_config_category_t *category = categories->data + startIndex;
	u32 favoriteCount = inheritedFavoriteCount + category->favorite ? 1u : 0u;
	u32 endIndex = startIndex + 1;
	while(endIndex < categories->count) {
		view_config_category_t *subCategory = categories->data + endIndex;
		if(subCategory->depth <= category->depth)
			break;
		favoriteCount += subCategory->favorite;
		++endIndex;
	}

	if(favorites && !favoriteCount)
		return endIndex;

	if(!favorites && category->favorite)
		return endIndex;

	category->visible = visible;

	if(startIndex + 1 != endIndex) {
		u32 index = startIndex + 1;
		while(index < endIndex) {
			index = view_set_favorite_config_category_visibility(view, index, favorites, visible, favoriteCount);
		}
	}

	return endIndex;
}

void view_set_favorite_category_visibility(view_t *view, b32 favorite, b8 visible)
{
	BB_LOG("Debug", "%s %sfavorite categories for '%s'\n",
	       visible ? "Checked" : "Unchecked",
	       favorite ? "" : "non-",
	       view->session->appInfo.packet.appInfo.applicationName);
	view->visibleLogsDirty = true;
	if(favorite) {
		view->config.newFavoriteColumnVisibility = visible;
		view->changedFavoriteColumnVisibility = true;
	} else {
		view->config.newNonFavoriteColumnVisibility = visible;
		view->changedNonFavoriteColumnVisibility = true;
	}

	u32 index = 0;
	while(index < view->config.configCategories.count) {
		index = view_set_favorite_config_category_visibility(view, index, favorite, visible, 0);
	}

	index = 0;
	while(index < view->categories.count) {
		index = view_set_favorite_category_visibility_recursive(view, index, favorite, visible, 0);
	}
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

void view_add_log(view_t *view, recorded_log_t *log)
{
	u32 i;
	u32 persistentLogIndex = view->persistentLogs.count;
	for(i = 0; i < log->numLines; ++i) {
		view_persistent_log_t *persistent = bba_add(view->persistentLogs, 1);
		persistent->sessionLogIndex = log->sessionLogIndex;
		persistent->subLine = i;
		persistent->bookmarked = false;
	}
	u32 visibleLogCount = view->visibleLogs.count;
	view_add_log_internal(view, log, persistentLogIndex);
	if(visibleLogCount < view->visibleLogs.count) {
		view->visibleLogsAdded = true;
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
