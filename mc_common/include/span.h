// Copyright (c) 2012-2020 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct span_s {
	const char *start;
	const char *end;
} span_t;

span_t span_from_string(const char *str);
int span_strcmp(span_t a, span_t b);
int span_stricmp(span_t a, span_t b);

typedef enum span_case_e {
	kSpanCaseSensitive,
	kSpanCaseInsentitive,
} span_case_e;

int span_starts_with(span_t span, const char *str, span_case_e spanCase);
int span_ends_with(span_t span, const char *str, span_case_e spanCase);

int span_is_empty(span_t span);
char span_peek(span_t span);
span_t span_pop_front(span_t span);
u64 span_length(span_t span);

#if defined(__cplusplus)
}
#endif
