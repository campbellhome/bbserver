// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"
#include "bb_types.h"

bb_log_level_e GetLogLevelBasedOnCounts(const u32 logCount[/*kBBLogLevel_Count*/]);
struct ImVec4 GetTextColorForLogLevel(u32 logLevel);

class LogLevelColorizer
{
public:
	LogLevelColorizer(bb_log_level_e logLevel);
	~LogLevelColorizer();
};
