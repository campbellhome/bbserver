// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "sb.h"

#if defined(__cplusplus)
extern "C" {
#endif

sb_t buildUtils_objectPathFromSourcePath(const char *objectDir, const char *sourcePath);
void buildUtils_appendObjects(const char *objectDir, const sbs_t *sourcePaths, sb_t *command);
sbs_t buildUtils_filterSourcesByExtension(const sbs_t *src, const char *ext);

#if defined(__cplusplus)
} // extern "C"
#endif
