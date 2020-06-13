// Copyright (c) 2012-2020 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"
#include "bb_types.h"
#include "theme_config.h"

bool PushTextShadows(styleColor_e styleColor);
void PopTextShadows(bool oldShadows);

class ScopedTextShadows
{
	bool bShadows = false;

public:
	ScopedTextShadows(bb_log_level_e logLevel);
	ScopedTextShadows(styleColor_e styleColor);
	~ScopedTextShadows();
};
