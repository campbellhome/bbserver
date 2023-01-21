// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "mc_build/mc_build_utils.h"
#include "bb_array.h"
#include "bb_string.h"
#include "path_utils.h"

sb_t buildUtils_objectPathFromSourcePath(const char* objectDir, const char* sourcePath)
{
	sb_t objectPath = { BB_EMPTY_INITIALIZER };
	const char* sep = strrchr(sourcePath, '\\');
	const char* ext = strrchr(sourcePath, '.');
	if (sep && ext > sep)
	{
		++sep;
		objectPath = sb_from_c_string(objectDir);
		sb_va(&objectPath, "/%.*s.o", ext - sep, sep);
		path_resolve_inplace(&objectPath);
	}
	return objectPath;
}

void buildUtils_appendObjects(const char* objectDir, const sbs_t* sourcePaths, sb_t* command)
{
	for (u32 i = 0; i < sourcePaths->count; ++i)
	{
		const sb_t* sourcePath = sourcePaths->data + i;
		sb_t objectPath = buildUtils_objectPathFromSourcePath(objectDir, sb_get(sourcePath));
		sb_replace_all_inplace(&objectPath, "\\", "/");
		sb_va(command, " %s", sb_get(&objectPath));
		sb_reset(&objectPath);
	}
}

sbs_t buildUtils_filterSourcesByExtension(const sbs_t* src, const char* ext)
{
	sbs_t dst = { BB_EMPTY_INITIALIZER };
	for (u32 i = 0; i < src->count; ++i)
	{
		const sb_t* str = src->data + i;
		const char* srcExt = strrchr(sb_get(str), '.');

		if (srcExt && !bb_stricmp(srcExt, ext))
		{
			bba_push(dst, sb_clone(str));
		}
	}
	return dst;
}
