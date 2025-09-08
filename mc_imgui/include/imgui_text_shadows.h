// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"
#include "bb_types.h"
#include "theme_config.h"

#if defined(__cplusplus)
extern "C"
{
#endif

void EnableTextShadows(b32 enabled);
b32 PushTextShadows(styleColor_e styleColor);
void PopTextShadows(b32 oldShadows);

#if defined(__cplusplus)
}

class ScopedTextShadows
{
	b32 bShadows = false;

public:
	ScopedTextShadows(bb_log_level_e logLevel);
	ScopedTextShadows(styleColor_e styleColor);
	~ScopedTextShadows();
};
#endif
