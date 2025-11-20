// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct sb_s sb_t;
typedef struct span_s span_t;
	
void BBServer_OpenDirInExplorer(const char* dir);

static const char kColorKeyPrefix = '^';
static const char kFirstColorKey = '0';
static const char kLastColorKey = '=';

enum
{
	kColorKeyOffset = 1
};

sb_t sb_expand_json(sb_t source);
void span_strip_color_codes(span_t span, sb_t* output);
b32 line_can_be_json(sb_t line);

#if defined(__cplusplus)
}
#endif
