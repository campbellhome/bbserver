// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "sdict.h"
#include "bb_array.h"
#include "bb_wrap_stdio.h"
#include "parson/parson.h"
#include <stdlib.h>

void sdict_init(sdict_t *sd)
{
	sd->count = sd->allocated = 0;
	sd->data = NULL;
	sd->unique = false;
}

void sdictEntry_reset(sdictEntry_t *e)
{
	sb_reset(&e->key);
	sb_reset(&e->value);
}

void sdict_reset(sdict_t *sd)
{
	for(u32 i = 0; i < sd->count; ++i) {
		sdictEntry_t *e = sd->data + i;
		sdictEntry_reset(e);
	}
	bba_free(*sd);
}

void sdict_move(sdict_t *target, sdict_t *src)
{
	sdict_reset(target);
	memcpy(target, src, sizeof(*src));
	memset(src, 0, sizeof(*src));
}

void sdict_copy(sdict_t *target, sdict_t *src)
{
	sdict_reset(target);
	for(u32 i = 0; i < src->count; ++i) {
		const sdictEntry_t *e = src->data + i;
		if(bba_add(*target, 1) != NULL) {
			sdictEntry_t *te = &bba_last(*target);
			sb_append(&te->key, sb_get(&e->key));
			sb_append(&te->value, sb_get(&e->value));
		}
	}
}

u32 sdict_find_index(sdict_t *sd, const char *key)
{
	for(u32 i = 0; i < sd->count; ++i) {
		sdictEntry_t *e = sd->data + i;
		if(!strcmp(key, sb_get(&e->key))) {
			return i;
		}
	}
	return ~0U;
}

u32 sdict_find_index_from(sdict_t *sd, const char *key, u32 startIndex)
{
	for(u32 i = startIndex; i < sd->count; ++i) {
		sdictEntry_t *e = sd->data + i;
		if(!strcmp(key, sb_get(&e->key))) {
			return i;
		}
	}
	return ~0U;
}

sdictEntry_t *sdict_find_entry(sdict_t *sd, const char *key)
{
	for(u32 i = 0; i < sd->count; ++i) {
		sdictEntry_t *e = sd->data + i;
		if(!strcmp(key, sb_get(&e->key))) {
			return e;
		}
	}
	return NULL;
}

const char *sdict_find(sdict_t *sd, const char *key)
{
	for(u32 i = 0; i < sd->count; ++i) {
		sdictEntry_t *e = sd->data + i;
		if(!strcmp(key, sb_get(&e->key))) {
			return sb_get(&e->value);
		}
	}
	return NULL;
}

const char *sdict_find_safe(sdict_t *sd, const char *key)
{
	const char *result = sdict_find(sd, key);
	if(!result) {
		result = "";
	}
	return result;
}

b32 sdict_grow(sdict_t *sd, u32 len)
{
	u32 originalCount = sd->count;
	u32 desiredCount = originalCount + len;
	bba_add_noclear(*sd, len);
	if(sd->data && sd->count == desiredCount) {
		return true;
	}
	return false;
}

b32 sdict_add(sdict_t *sd, sdictEntry_t *entry)
{
	if(sd->unique) {
		sdict_remove(sd, sb_get(&entry->key));
	}
	if(sdict_grow(sd, 1)) {
		sdictEntry_t *e = sd->data + sd->count - 1;
		*e = *entry;
		memset(entry, 0, sizeof(*entry));
		return true;
	}
	sdictEntry_reset(entry);
	return false;
}

b32 sdict_add_raw(sdict_t *sd, const char *key, const char *value)
{
	sdictEntry_t e = { BB_EMPTY_INITIALIZER };
	sb_append(&e.key, key);
	sb_append(&e.value, value);
	return sdict_add(sd, &e);
}

b32 sdict_remove(sdict_t *sd, const char *key)
{
	for(u32 i = 0; i < sd->count; ++i) {
		sdictEntry_t *e = sd->data + i;
		if(e->key.data && !strcmp(key, e->key.data)) {
			u32 lastIndex = sd->count - 1;
			sdictEntry_reset(e);
			if(lastIndex != i) {
				*e = sd->data[lastIndex];
			}
			--sd->count;
			return true;
		}
	}
	return false;
}

int sdict_compare(const void *_a, const void *_b)
{
	sdictEntry_t *a = (sdictEntry_t *)_a;
	sdictEntry_t *b = (sdictEntry_t *)_b;
	return strcmp(sb_get(&a->key), sb_get(&b->key));
}

void sdict_sort(sdict_t *sd)
{
	qsort(sd->data, sd->count, sizeof(sdictEntry_t), &sdict_compare);
}

void sdicts_init(sdicts *sds)
{
	sds->count = sds->allocated = 0;
	sds->data = NULL;
}

void sdicts_reset(sdicts *sds)
{
	for(u32 i = 0; i < sds->count; ++i) {
		sdict_reset(sds->data + i);
	}
	bba_free(*sds);
}

void sdicts_move(sdicts *target, sdicts *src)
{
	sdicts_reset(target);
	memcpy(target, src, sizeof(*src));
	memset(src, 0, sizeof(*src));
}

JSON_Value *json_serialize_sdictEntry_t(const sdictEntry_t *src)
{
	JSON_Value *val = json_value_init_object();
	JSON_Object *obj = json_value_get_object(val);
	if(obj) {
		json_object_set_value(obj, "key", json_serialize_sb_t(&src->key));
		json_object_set_value(obj, "value", json_serialize_sb_t(&src->value));
	}
	return val;
}

sdictEntry_t json_deserialize_sdictEntry_t(JSON_Value *src)
{
	sdictEntry_t dst;
	memset(&dst, 0, sizeof(dst));
	if(src) {
		JSON_Object *obj = json_value_get_object(src);
		if(obj) {
			dst.key = json_deserialize_sb_t(json_object_get_value(obj, "key"));
			dst.value = json_deserialize_sb_t(json_object_get_value(obj, "value"));
		}
	}
	return dst;
}

JSON_Value *json_serialize_sdict_t(const sdict_t *src)
{
	JSON_Value *val;
	if(src->unique) {
		val = json_value_init_object();
		JSON_Object *obj = json_value_get_object(val);
		if(obj) {
			for(u32 i = 0; i < src->count; ++i) {
				const sdictEntry_t *e = src->data + i;
				json_object_set_value(obj, sb_get(&e->key), json_serialize_sb_t(&e->value));
			}
		}
	} else {
		val = json_value_init_array();
		JSON_Array *arr = json_value_get_array(val);
		if(arr) {
			for(u32 i = 0; i < src->count; ++i) {
				json_array_append_value(arr, json_serialize_sdictEntry_t(src->data + i));
			}
		}
	}
	return val;
}

sdict_t json_deserialize_sdict_t(JSON_Value *src)
{
	sdict_t dst;
	sdict_init(&dst);
	JSON_Object *obj = json_value_get_object(src);
	if(obj) {
		size_t count = json_object_get_count(obj);
		for(size_t i = 0; i < count; ++i) {
			const char *key = json_object_get_name(obj, i);
			JSON_Value *val = json_object_get_value_at(obj, i);
			const char *valStr = json_value_get_string(val);
			if(key && val) {
				sdictEntry_t e;
				sb_init(&e.key);
				sb_append(&e.key, key);
				sb_init(&e.value);
				sb_append(&e.value, valStr);
				sdict_add(&dst, &e);
			}
		}
		dst.unique = true;
	} else {
		JSON_Array *arr = json_value_get_array(src);
		if(arr) {
			size_t count = json_array_get_count(arr);
			for(size_t i = 0; i < count; ++i) {
				JSON_Object *arrObj = json_array_get_object(arr, i);
				const char *key = json_object_get_string(arrObj, "key");
				const char *val = json_object_get_string(arrObj, "value");
				if(key && val) {
					sdictEntry_t e;
					sb_init(&e.key);
					sb_append(&e.key, key);
					sb_init(&e.value);
					sb_append(&e.value, val);
					sdict_add(&dst, &e);
				}
			}
		}
	}
	return dst;
}
