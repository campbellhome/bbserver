// Copyright (c) 2012-2021 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "config.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct recorded_session_s recorded_session_t;
typedef struct view_s view_t;
typedef struct view_category_s view_category_t;
typedef struct view_thread_s view_thread_t;
typedef struct view_file_s view_file_t;
typedef struct view_config_category_s view_config_category_t;
typedef struct view_config_thread_s view_config_thread_t;
typedef struct view_config_file_s view_config_file_t;

b32 view_config_write(view_t* view);
b32 view_config_read(view_t* view);
void view_config_add_categories_to_session(recorded_session_t* session);

void view_apply_config_category(const view_config_category_t* cc, view_category_t* vc);

void view_apply_config_thread(view_config_thread_t* ct, view_thread_t* vt);
view_config_thread_t* view_find_config_thread(view_t* view, const char* name);

void view_apply_config_file(view_config_file_t* ct, view_file_t* vt);
view_config_file_t* view_find_config_file(view_t* view, const char* name);

sb_t view_session_config_get_path(const char* sessionPath);

#if defined(__cplusplus)
}
#endif
