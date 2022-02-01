// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct tag_mc_updater_filenames {
	u32 count;
	u32 allocated;
	const char **data;
} mc_updater_filenames;

typedef struct tag_mc_updater_globals {
	const char *appName;
	const char *appdataName;
	const char *currentVersionJsonFilename;
	const char *p4VersionDir;
	const char *manifestFilename;
	mc_updater_filenames contentsFilenames;
} mc_updater_globals;

b32 mc_updater_main(mc_updater_globals *globals);

#if defined(__cplusplus)
}
#endif
