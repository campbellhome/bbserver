// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "view_config.h"
#include "appdata.h"
#include "bb_array.h"
#include "bb_json_generated.h"
#include "bb_string.h"
#include "bb_structs_generated.h"
#include "bb_wrap_stdio.h"
#include "file_utils.h"
#include "line_parser.h"
#include "recorded_session.h"
#include "recordings.h"
#include "sb.h"
#include "va.h"
#include "view.h"
#include <stdlib.h>

static sb_t view_config_get_path(view_t *view, const char *appName)
{
	recording_t *recording = recordings_find_by_path(view->session->path);
	const char *filename = view->session->applicationFilename;
	sb_t s = appdata_get(appName);
	if(recording && recording->recordingType == kRecordingType_ExternalFile) {
		sb_va(&s, "\\bb_external_view_%s.json", filename);
	} else {
		sb_va(&s, "\\bb_view_%s.json", filename);
	}
	return s;
}

void get_appdata_folder(char *buffer, size_t bufferSize);

static char *escape_string(const char *src)
{
	sb_t sb;
	char *ret;
	sb_init(&sb);
	while(*src) {
		if(*src == '\\') {
			sb_append(&sb, "\\\\");
		} else if(*src == '\r') {
			sb_append(&sb, "\\r");
		} else if(*src == '\n') {
			sb_append(&sb, "\\n");
		} else if(*src == '\t') {
			sb_append(&sb, "\\t");
		} else if(*src == '\"') {
			sb_append(&sb, "\\'");
		} else {
			sb_append_char(&sb, *src);
		}
		++src;
	}
	ret = va("%s", sb_get(&sb));
	sb_reset(&sb);
	return ret;
}

static char *unescape_string(const char *src)
{
	sb_t sb;
	char *ret;
	sb_init(&sb);
	while(*src) {
		if(*src == '\\') {
			++src;
			switch(*src) {
			case '\\':
				sb_append_char(&sb, '\\');
				break;
			case 'r':
				sb_append_char(&sb, '\r');
				break;
			case 'n':
				sb_append_char(&sb, '\n');
				break;
			case 't':
				sb_append_char(&sb, '\t');
				break;
			case '\'':
				sb_append_char(&sb, '\"');
				break;
			}
		} else {
			sb_append_char(&sb, *src);
		}
		++src;
	}
	ret = va("%s", sb_get(&sb));
	sb_reset(&sb);
	return ret;
}

static b32 view_get_config_path(view_t *view, char *buffer, size_t bufferSize)
{
	const char *filename = view->session->applicationFilename;
	size_t dirLen;
	get_appdata_folder(buffer, bufferSize);
	dirLen = strlen(buffer);

	if(bb_snprintf(buffer + dirLen, bufferSize - dirLen, "\\%s.bbview", filename) < 0)
		return false;

	return true;
}

view_config_category_t *view_find_config_category(view_t *view, const char *name)
{
	u32 i;
	for(i = 0; i < view->config.configCategories.count; ++i) {
		view_config_category_t *c = view->config.configCategories.data + i;
		if(!strcmp(sb_get(&c->name), name))
			return c;
	}
	return NULL;
}
static view_config_category_t *view_find_or_add_config_category(view_t *view, const char *name)
{
	view_config_category_t *c = view_find_config_category(view, name);
	if(!c) {
		c = bba_add(view->config.configCategories, 1);
		if(c) {
			sb_append(&c->name, name);
		}
	}
	return c;
}
static int ViewConfigCategoryCompare(const void *_a, const void *_b)
{
	const view_config_category_t *a = (const view_config_category_t *)_a;
	const view_config_category_t *b = (const view_config_category_t *)_b;
	return strcmp(sb_get(&a->name), sb_get(&b->name));
}
void view_apply_config_category(view_t *view, view_config_category_t *cc, view_category_t *vc)
{
	u32 i;
	u32 startIndex = (u32)(vc - view->categories.data);
	u32 endIndex = startIndex + 1;
	while(endIndex < view->categories.count) {
		view_category_t *subCategory = view->categories.data + endIndex;
		if(subCategory->depth <= vc->depth)
			break;
		++endIndex;
	}
	for(i = startIndex; i < endIndex; ++i) {
		vc = view->categories.data + i;
		vc->visible = cc->visible;
		vc->selected = cc->selected;
		vc->favorite = cc->favorite;
		//BB_LOG("Config::Category", "%s apply for '%s'", view->session->applicationFilename, vc->categoryName);
	}
}

view_config_thread_t *view_find_config_thread(view_t *view, const char *name)
{
	u32 i;
	for(i = 0; i < view->config.configThreads.count; ++i) {
		view_config_thread_t *t = view->config.configThreads.data + i;
		if(!strcmp(sb_get(&t->name), name))
			return t;
	}
	return NULL;
}
static view_config_thread_t *view_find_or_add_config_thread(view_t *view, const char *name)
{
	view_config_thread_t *t = view_find_config_thread(view, name);
	if(!t) {
		t = bba_add(view->config.configThreads, 1);
		if(t) {
			sb_append(&t->name, name);
		}
	}
	return t;
}
static int ViewConfigThreadCompare(const void *_a, const void *_b)
{
	const view_config_thread_t *a = (const view_config_thread_t *)_a;
	const view_config_thread_t *b = (const view_config_thread_t *)_b;
	return strcmp(sb_get(&a->name), sb_get(&b->name));
}
void view_apply_config_thread(view_config_thread_t *ct, view_thread_t *vt)
{
	vt->visible = ct->visible;
	vt->selected = ct->selected;
}

view_config_file_t *view_find_config_file(view_t *view, const char *name)
{
	u32 i;
	for(i = 0; i < view->config.configFiles.count; ++i) {
		view_config_file_t *t = view->config.configFiles.data + i;
		if(!strcmp(sb_get(&t->path), name))
			return t;
	}
	return NULL;
}
static view_config_file_t *view_find_or_add_config_file(view_t *view, const char *name)
{
	view_config_file_t *t = view_find_config_file(view, name);
	if(!t) {
		t = bba_add(view->config.configFiles, 1);
		if(t) {
			sb_append(&t->path, name);
		}
	}
	return t;
}
static int ViewConfigFileCompare(const void *_a, const void *_b)
{
	const view_config_file_t *a = (const view_config_file_t *)_a;
	const view_config_file_t *b = (const view_config_file_t *)_b;
	return strcmp(sb_get(&a->path), sb_get(&b->path));
}
void view_apply_config_file(view_config_file_t *ct, view_file_t *vt)
{
	vt->visible = ct->visible;
	vt->selected = ct->selected;
}

static void view_config_update_category_depth(view_t *view)
{
	for(u32 i = 0; i < view->config.configCategories.count; ++i) {
		view_config_category_t *c = view->config.configCategories.data + i;
		c->depth = 0;
		for(const char *s = c->name.data; s && *s; ++s) {
			if(s[0] == ':' && s[1] == ':') {
				++c->depth;
				++s; // ::: only counts as 1 match
			}
		}
	}
}
static void view_config_write_prep(view_t *view)
{
	view_console_history_reset(&view->config.consoleHistory);
	view->config.consoleHistory = view_console_history_clone(&view->consoleHistory);

	for(u32 i = 0; i < view->categories.count; ++i) {
		view_category_t *vc = view->categories.data + i;
		view_config_category_t *cc = view_find_or_add_config_category(view, vc->categoryName);
		if(cc) {
			cc->selected = vc->selected;
			cc->visible = vc->visible;
			cc->favorite = vc->favorite;
		}
	}
	qsort(view->config.configCategories.data, view->config.configCategories.count, sizeof(view->config.configCategories.data[0]), ViewConfigCategoryCompare);
	view_config_update_category_depth(view);
	for(u32 i = 0; i < view->threads.count; ++i) {
		view_thread_t *vt = view->threads.data + i;
		if(!vt->visible) {
			view_config_thread_t *ct = view_find_or_add_config_thread(view, vt->threadName);
			if(ct) {
				ct->selected = vt->selected;
				ct->visible = vt->visible;
			}
		}
	}
	for(u32 i = 0; i < view->config.configThreads.count;) {
		view_config_thread_t *ct = view->config.configThreads.data + i;
		if(ct->visible) {
			sb_reset(&ct->name);
			bba_erase(view->config.configThreads, i);
		} else {
			++i;
		}
	}
	for(u32 i = 0; i < view->files.count; ++i) {
		view_file_t *vf = view->files.data + i;
		if(!vf->visible) {
			view_config_file_t *ct = view_find_or_add_config_file(view, vf->path);
			if(ct) {
				ct->selected = vf->selected;
				ct->visible = vf->visible;
			}
		}
	}
	for(u32 i = 0; i < view->config.configFiles.count;) {
		view_config_file_t *cf = view->config.configFiles.data + i;
		if(cf->visible) {
			sb_reset(&cf->path);
			bba_erase(view->config.configFiles, i);
		} else {
			++i;
		}
	}
	view_config_columns_reset(&view->config.configColumns);
	for(u32 i = 0; i < kColumn_Count; ++i) {
		if(bba_add(view->config.configColumns, 1)) {
			view_config_column_t *cc = &bba_last(view->config.configColumns);
			sb_append(&cc->name, g_view_column_config_names[i]);
			cc->visible = view->columns[i].visible;
			cc->width = view->columns[i].width;
		}
	}
}

static void view_config_read_fixup(view_t *view)
{
	view_console_history_reset(&view->consoleHistory);
	view->consoleHistory = view_console_history_clone(&view->config.consoleHistory);

	view->visibleLogsDirty = true;

	BB_LOG("view::config", "read fixup sort configFiles");
	qsort(view->config.configFiles.data, view->config.configFiles.count, sizeof(view->config.configFiles.data[0]), ViewConfigFileCompare);
	for(u32 vfi = 0; vfi < view->files.count; ++vfi) {
		view_file_t *vf = view->files.data + vfi;
		for(u32 cfi = 0; cfi < view->config.configFiles.count; ++cfi) {
			view_config_file_t *cf = view->config.configFiles.data + cfi;
			if(!strcmp(vf->path, sb_get(&cf->path))) {
				BB_LOG("view::config", "read fixup apply configFile %s", vf->path);
				view_apply_config_file(cf, vf);
				break;
			}
		}
	}

	BB_LOG("view::config", "read fixup sort configThreads");
	qsort(view->config.configThreads.data, view->config.configThreads.count, sizeof(view->config.configThreads.data[0]), ViewConfigThreadCompare);
	for(u32 vci = 0; vci < view->threads.count; ++vci) {
		view_thread_t *vt = view->threads.data + vci;
		for(u32 cti = 0; cti < view->config.configThreads.count; ++cti) {
			view_config_thread_t *ct = view->config.configThreads.data + cti;
			if(!strcmp(vt->threadName, sb_get(&ct->name))) {
				BB_LOG("view::config", "read fixup apply configThread %s", vt->threadName);
				view_apply_config_thread(ct, vt);
				break;
			}
		}
	}

	BB_LOG("view::config", "read fixup sort configCategories");
	qsort(view->config.configCategories.data, view->config.configCategories.count, sizeof(view->config.configCategories.data[0]), ViewConfigCategoryCompare);
	view_config_update_category_depth(view);
	for(u32 vci = 0; vci < view->categories.count; ++vci) {
		view_category_t *vc = view->categories.data + vci;
		for(u32 cci = 0; cci < view->config.configCategories.count; ++cci) {
			view_config_category_t *cc = view->config.configCategories.data + cci;
			if(!strcmp(vc->categoryName, sb_get(&cc->name))) {
				BB_LOG("view::config", "read fixup apply configCategory %s", vc->categoryName);
				view_apply_config_category(view, cc, vc);
				break;
			}
		}
	}

	BB_LOG("view::config", "read fixup columns");
	for(u32 ci = 0; ci < view->config.configColumns.count; ++ci) {
		view_config_column_t *cc = view->config.configColumns.data + ci;
		const char *cname = sb_get(&cc->name);
		for(u32 vi = 0; vi < kColumn_Count; ++vi) {
			if(!strcmp(cname, g_view_column_config_names[vi])) {
				view->columns[vi].visible = cc->visible;
				view->columns[vi].width = cc->width;
				break;
			}
		}
	}
}

b32 view_config_write(view_t *view)
{
	sb_t path = view_config_get_path(view, "bb");
	BB_LOG("view::config", "write config to %s", sb_get(&path));
	view_config_write_prep(view);
	b32 result = false;
	JSON_Value *val = json_serialize_view_config_t(&view->config);
	if(val) {
		FILE *fp = fopen(sb_get(&path), "wb");
		if(fp) {
			char *serialized_string = json_serialize_to_string_pretty(val);
			fputs(serialized_string, fp);
			fclose(fp);
			json_free_serialized_string(serialized_string);
			result = true;
		}
	}
	json_value_free(val);
	sb_reset(&path);
	BB_LOG("view::config", "write config done");
	return result;
}

b32 view_config_read(view_t *view)
{
	b32 ret = false;
	sb_t path = view_config_get_path(view, "bb");
	JSON_Value *val = json_parse_file(sb_get(&path));
	if(val) {
		BB_LOG("view::config", "read config from %s", sb_get(&path));
		view->config = json_deserialize_view_config_t(val);
		json_value_free(val);
		ret = true;
	}
	sb_reset(&path);

	view->config.version = kViewConfigVersion;
	view_config_read_fixup(view);
	BB_LOG("view::config", "read config done");
	return ret;
}
