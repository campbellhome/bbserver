// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include "sb.h"

AUTOJSON AUTOFROMLOC typedef struct sbsHashEntry {
	sb_t key;
	sbs_t values;
} sbsHashEntry;

AUTOSTRUCT AUTOFROMLOC typedef struct sbsHashChain {
	u32 count;
	u32 allocated;
	sbsHashEntry *data;
} sbsHashChain;

AUTOSTRUCT AUTOFROMLOC AUTOSTRINGHASH typedef struct sbsHashTable {
	u32 count;
	u32 allocated;
	sbsHashChain *data;
} sbsHashTable;

AUTOJSON AUTOFROMLOC typedef struct tag_s {
	sb_t name;
	sbs_t categories;
} tag_t;

AUTOJSON AUTOFROMLOC typedef struct tags_s {
	u32 count;
	u32 allocated;
	tag_t *data;
} tags_t;

AUTOSTRUCT AUTOFROMLOC typedef struct tagCategory_s {
	sb_t name;
	sbs_t tags;
} tagCategory_t;

AUTOSTRUCT AUTOFROMLOC typedef struct tagCategories_s {
	u32 count;
	u32 allocated;
	tagCategory_t *data;
} tagCategories_t;

AUTOSTRUCT typedef struct tagData_s {
	sbsHashTable tagTable;
	sbsHashTable categoryTable;
	tags_t tags;
	tagCategories_t categories;
} tagData_t;

AUTOJSON typedef struct tags_config_s {
	tags_t tags;
} tags_config_t;

extern tagData_t g_tags;

void tags_init(void);
void tags_write(void);
void tags_shutdown(void);

tag_t *tag_find(const char *tagName);
void tag_remove(tag_t *tag);

tagCategory_t *tagCategory_find(const char *categoryName);

void tag_add_category(const char *tagName, const char *categoryName);
void tag_remove_category(const char *tagName, const char *categoryName);

#if defined(__cplusplus)
}
#endif
