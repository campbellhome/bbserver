// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "config.h"

typedef struct view_s view_t;
typedef struct view_category_s view_category_t;
typedef struct view_thread_s view_thread_t;
typedef struct view_file_s view_file_t;
typedef struct view_config_category_s view_config_category_t;
typedef struct view_config_thread_s view_config_thread_t;
typedef struct view_config_file_s view_config_file_t;

b32 view_config_write(view_t *view);
b32 view_config_read(view_t *view);

void view_apply_config_category(view_t *view, view_config_category_t *cc, view_category_t *vc);
view_config_category_t *view_find_config_category(view_t *view, const char *name);

void view_apply_config_thread(view_config_thread_t *ct, view_thread_t *vt);
view_config_thread_t *view_find_config_thread(view_t *view, const char *name);

void view_apply_config_file(view_config_file_t *ct, view_file_t *vt);
view_config_file_t *view_find_config_file(view_t *view, const char *name);
