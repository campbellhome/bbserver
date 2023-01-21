// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include <stdarg.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct span_s span_t;

AUTOJSON AUTOHEADERONLY AUTOFROMLOC typedef struct sb_s
{
	u32 count;
	u32 allocated;
	char* data;
} sb_t; // string builder

AUTOJSON AUTOHEADERONLY AUTOFROMLOC typedef struct sbs_s
{
	u32 count;
	u32 allocated;
	sb_t* data;
} sbs_t;

#define sb_from_span(src) sb_from_span_from_loc(__FILE__, __LINE__, src)
#define sb_from_c_string(src) sb_from_c_string_from_loc(__FILE__, __LINE__, src)
#if !defined(sb_clone)
#define sb_clone(sb) sb_clone_from_loc(__FILE__, __LINE__, sb)
#define sb_reset(sb) sb_reset_from_loc(__FILE__, __LINE__, sb)
#endif
#define sb_reserve(sb, len) sb_reserve_from_loc(__FILE__, __LINE__, sb, len)
#define sb_grow(sb, len) sb_grow_from_loc(__FILE__, __LINE__, sb, len)
#define sb_move(target, src) sb_move_from_loc(__FILE__, __LINE__, target, src)
#define sb_append(sb, text) sb_append_from_loc(__FILE__, __LINE__, sb, text)
#define sb_append_range(sb, start, end) sb_append_range_from_loc(__FILE__, __LINE__, sb, start, end)
#define sb_append_char(sb, c) sb_append_char_from_loc(__FILE__, __LINE__, sb, c)
#define sb_from_va(fmt, ...) sb_from_va_from_loc(__FILE__, __LINE__, fmt, __VA_ARGS__)
#define sb_va(sb, fmt, ...) sb_va_from_loc(__FILE__, __LINE__, sb, fmt, __VA_ARGS__)
#define sb_va_list(sb, fmt, args) sb_va_list_from_loc(__FILE__, __LINE__, sb, fmt, args)
#define sb_replace_all(sb, replaceThis, replacement) sb_replace_all_from_loc(__FILE__, __LINE__, sb, replaceThis, replacement)
#define sb_replace_all_inplace(sb, replaceThis, replacement) sb_replace_all_inplace_from_loc(__FILE__, __LINE__, sb, replaceThis, replacement)

void sb_init(sb_t* sb);
void sb_reset_from_loc(const char* file, int line, sb_t* sb);
void sb_clear(sb_t* sb);
u32 sb_len(sb_t* sb);
void sb_replace_char_inplace(sb_t* sb, char src, char dst);
sb_t sb_from_span_from_loc(const char* file, int line, span_t span);
sb_t sb_from_c_string_from_loc(const char* file, int line, const char* src);
sb_t sb_from_c_string_no_alloc(const char* src);
sb_t sb_clone_from_loc(const char* file, int line, const sb_t* src);
b32 sb_reserve_from_loc(const char* file, int line, sb_t* sb, u32 len);
b32 sb_grow_from_loc(const char* file, int line, sb_t* sb, u32 len);
void sb_move_from_loc(const char* file, int line, sb_t* target, sb_t* src);
void sb_append_from_loc(const char* file, int line, sb_t* sb, const char* text);
void sb_append_range_from_loc(const char* file, int line, sb_t* sb, const char* start, const char* end);
void sb_append_char_from_loc(const char* file, int line, sb_t* sb, char c);
sb_t sb_from_va_from_loc(const char* file, int line, const char* fmt, ...);
void sb_va_from_loc(const char* file, int line, sb_t* sb, const char* fmt, ...);
void sb_va_list_from_loc(const char* file, int line, sb_t* sb, const char* fmt, va_list args);
sb_t sb_replace_all_from_loc(const char* file, int line, const sb_t* src, const char* replaceThis, const char* replacement);
void sb_replace_all_inplace_from_loc(const char* file, int line, sb_t* src, const char* replaceThis, const char* replacement);
void sb_toupper_inline(sb_t* s);
void sb_tolower_inline(sb_t* s);
const char* sb_get(const sb_t* sb);

// NOTE: case-sensitive
int sbs_sort(const void* _a, const void* _b);
void sbs_add_unique_from_loc(const char* file, int line, sbs_t* sbs, sb_t value);
void sbs_remove_unique(sbs_t* sbs, sb_t value);
b32 sbs_contains(sbs_t* sbs, sb_t value);
#define sbs_add_unique(sbs, value) sbs_add_unique_from_loc(__FILE__, __LINE__, (sbs), (value))

#if !defined(sbs_clone)
#define sbs_clone(sbs) sbs_clone_from_loc(__FILE__, __LINE__, sbs)
#define sbs_reset(sbs) sbs_reset_from_loc(__FILE__, __LINE__, sbs)
#endif

sbs_t sbs_clone_from_loc(const char* file, int line, const sbs_t* sbs);
void sbs_reset_from_loc(const char* file, int line, sbs_t* sbs);

#define sbs_from_tokenize(bufferCursor, delimiters) sbs_from_tokenize_from_loc(__FILE__, __LINE__, bufferCursor, delimiters);
sbs_t sbs_from_tokenize_from_loc(const char* file, int line, const char** bufferCursor, const char* delimiters);

typedef struct json_value_t JSON_Value;
sb_t json_deserialize_sb_t(JSON_Value* src);
JSON_Value* json_serialize_sb_t(const sb_t* src);
sbs_t json_deserialize_sbs_t(JSON_Value* src);
JSON_Value* json_serialize_sbs_t(const sbs_t* src);

#if defined(__cplusplus)
}
#endif
