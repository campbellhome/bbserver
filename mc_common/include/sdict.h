// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "sb.h"

#if defined(__cplusplus)
extern "C" {
#endif

AUTOJSON AUTOHEADERONLY typedef struct sdictEntry_s {
	sb_t key;
	sb_t value;
} sdictEntry_t;

AUTOJSON AUTOHEADERONLY typedef struct sdict_s {
	u32 count;
	u32 allocated;
	sdictEntry_t *data;
	b32 unique;
	u32 pad;
} sdict_t;

AUTOJSON AUTOHEADERONLY typedef struct tag_sdicts {
	u32 count;
	u32 allocated;
	sdict_t *data;
} sdicts;

void sdict_init(sdict_t *sd);
void sdict_reset(sdict_t *sd);
void sdictEntry_reset(sdictEntry_t *e);
void sdict_move(sdict_t *target, sdict_t *src);
void sdict_copy(sdict_t *target, const sdict_t *src);
sdict_t sdict_clone(const sdict_t *src);
u32 sdict_find_index(sdict_t *sd, const char *key);
u32 sdict_find_index_from(sdict_t *sd, const char *key, u32 startIndex);
sdictEntry_t *sdict_find_entry(sdict_t *sd, const char *key);
const char *sdict_find(sdict_t *sd, const char *key);
const char *sdict_find_safe(sdict_t *sd, const char *key);
b32 sdict_grow(sdict_t *sd, u32 len);
b32 sdict_add(sdict_t *sd, sdictEntry_t *entry);
b32 sdict_add_raw(sdict_t *sd, const char *key, const char *value);
b32 sdict_remove(sdict_t *sd, const char *key);
void sdict_sort(sdict_t *sd);

void sdicts_init(sdicts *sds);
void sdicts_reset(sdicts *sds);
sdicts sdicts_clone(const sdicts *src);
void sdicts_move(sdicts *target, sdicts *src);

typedef struct json_value_t JSON_Value;
JSON_Value *json_serialize_sdictEntry_t(const sdictEntry_t *src);
sdictEntry_t json_deserialize_sdictEntry_t(JSON_Value *src);
JSON_Value *json_serialize_sdict_t(const sdict_t *src);
sdict_t json_deserialize_sdict_t(JSON_Value *src);

#if defined(__cplusplus)
}
#endif
