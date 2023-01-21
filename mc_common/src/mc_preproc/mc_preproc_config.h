// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#define _HAS_EXCEPTIONS 0
#define _ITERATOR_DEBUG_LEVEL 0

#include "bb_common.h"
#include <string>
#include <vector>

struct preprocInputDir
{
	std::string dir;
	std::string base;
};

struct preprocInputConfig
{
	b32 checkFonts = false;
	std::vector<preprocInputDir> sourceDirs;
	std::vector<preprocInputDir> includeDirs;
};

struct preprocOutputConfig
{
	std::string prefix;
	std::string sourceDir;
	std::string includeDir;
	std::string baseDir;
};

struct preprocConfig
{
	b32 bb = false;
	preprocInputConfig input;
	preprocOutputConfig output;
};

preprocConfig read_preprocConfig(const char* path);
