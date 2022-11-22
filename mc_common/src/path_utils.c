// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "path_utils.h"
#include "bb_array.h"
#include "bb_string.h"
#include "bb_wrap_stdio.h"
#include "va.h"

static const char *errno_str(int e);

    char path_get_separator(void)
{
#if BB_USING(BB_PLATFORM_WINDOWS)
	return '\\';
#else
	return '/';
#endif
}

const char *path_get_filename(const char *path)
{
	const char *sep = strrchr(path, '/');
	if(sep)
		return sep + 1;
	sep = strrchr(path, '\\');
	if(sep)
		return sep + 1;
	return path;
}

const char *path_get_dir(const char *path)
{
	const char *filename = path_get_filename(path);
	if(filename <= path)
		return "";
	return va("%.*s", filename - path, path);
}

b32 path_validate_filename(const char *filename)
{
	while(*filename) {
		if(strchr("\"\':/\\%", *filename) != NULL)
			return false;
		++filename;
	}
	return true;
}

void path_fix_separators(char *path, char separator)
{
	while(path && *path) {
		if(*path == '/' || *path == '\\') {
			*path = separator;
		}
		++path;
	}
}

sb_t path_resolve(sb_t src)
{
	sb_t dst = sb_clone(&src);
	path_resolve_inplace(&dst);
	return dst;
}

void path_add_component(sb_t *path, const char *component)
{
	if(path->count > 1 && path->data[path->count - 2] != '/' && path->data[path->count - 2] != '\\') {
		sb_append_char(path, path_get_separator());
	}
	sb_append(path, component);
}

void path_resolve_inplace(sb_t *path)
{
	if(!path->data)
		return;

	const char *cursor = path->data;
	sbs_t pathTokens = sbs_from_tokenize(&cursor, "/\\");

	path->count = 0;
	if(path->data[0] == '/' || path->data[0] == '\\') {
		++path->count;
		if(path->data[1] == '/' || path->data[1] == '\\') {
			++path->count;
		}
	}
	path->data[path->count] = '\0';
	++path->count;

	u32 i = 0;
	while(i < pathTokens.count) {
		const char *cur = sb_get(pathTokens.data + i);
		if(!bb_stricmp(cur, ".")) {
			sb_reset(pathTokens.data + i);
			bba_erase(pathTokens, i);
			if(i > 0) {
				--i;
			}
		} else if(i < pathTokens.count - 1 && bb_stricmp(cur, "..") && !bb_stricmp(sb_get(pathTokens.data + i + 1), "..")) {
			sb_reset(pathTokens.data + i);
			bba_erase(pathTokens, i);
			sb_reset(pathTokens.data + i);
			bba_erase(pathTokens, i);
			if(i > 0) {
				--i;
			}
		} else {
			++i;
		}
	}

	for(i = 0; i < pathTokens.count; ++i) {
		path_add_component(path, sb_get(pathTokens.data + i));
	}

	sbs_reset(&pathTokens);
}

b32 path_test_resolve(void)
{
	const char *testPathStr = "../../../../program/some//other\\path/../mc_im/././gui/submodules/../../mc_common/include/./..";
	sb_t testPath = sb_from_c_string(testPathStr);
	path_resolve_inplace(&testPath);
	sb_replace_char_inplace(&testPath, '\\', '/');
	b32 bSuccess = !strcmp(sb_get(&testPath), "../../../../program/some/other/mc_im/mc_common");
	if(!bSuccess) {
		BB_ERROR("Path", "Path failed to resolve:\n%s\n%s", testPathStr, sb_get(&testPath));
	}
	sb_reset(&testPath);
	return bSuccess;
}

void path_remove_filename(sb_t *path)
{
	if(!path || !path->data)
		return;

	char *forwardSlash = strrchr(path->data, '/');
	char *backslash = strrchr(path->data, '\\');
	char *sep = forwardSlash > backslash ? forwardSlash : backslash;
	if(sep) {
		*sep = 0;
		path->count = (u32)(sep - path->data) + 1;
	} else {
		path->data[0] = 0;
		path->count = 0;
	}
}

#if BB_USING(BB_PLATFORM_WINDOWS)
#include <direct.h>
b32 path_mkdir(const char *path)
{
	b32 success = true;
	char *temp = _strdup(path);
	char *s = temp;
	while(*s) {
		if(*s == '/' || *s == '\\') {
			char c = *s;
			*s = '\0';
			if(s - temp > 2) {
				if(_mkdir(temp) == -1) {
					if(errno != EEXIST) {
						success = false;
					}
				}
			}
			*s = c;
		}
		++s;
	}
	free(temp);
	if(_mkdir(path) == -1) {
		if(errno != EEXIST) {
			success = false;
		}
	}
	return success;
}
b32 path_rmdir(const char *path)
{
	int ret = _rmdir(path);
	if(ret == 0) {
		return true;
	}
	int err = errno;
	BB_WARNING("mkdir", "rmdir '%s' returned %d (errno %d %s)\n", path, ret, err, errno_str(err));
	return false;
}
#else
#include <errno.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

b32 path_mkdir_norecurse(const char *path)
{
	mode_t process_mask = umask(0);
	int ret = mkdir(path, S_IRWXU | S_IRWXG);
	int err = errno;
	if(ret && err != EEXIST) {
		BB_WARNING("mkdir", "mkdir '%s' returned %d (errno %d %s)\n", path, ret, err, errno_str(err));
	}
	umask(process_mask);
	return !ret || err == EEXIST; // not completely correct as EEXIST could be a file
}

b32 path_mkdir(const char *path)
{
	char *temp = strdup(path);
	char *s = temp;
	while(*s) {
		if(*s == '/') {
			char c = *s;
			*s = '\0';
			if(s - temp > 2) {
				path_mkdir_norecurse(temp);
			}
			*s = c;
		}
		++s;
	}
	free(temp);
	return path_mkdir_norecurse(path);
}
b32 path_rmdir(const char *path)
{
	int ret = rmdir(path);
	if(ret == 0) {
		return true;
	}
	int err = errno;
	BB_WARNING("mkdir", "rmdir '%s' returned %d (errno %d %s)\n", path, ret, err, errno_str(err));
	return false;
}
#endif

static const char *errno_str(int e)
{
	switch(e) {
#define CASE(x) \
	case x:     \
		return #x
		CASE(EACCES);
		CASE(EEXIST);
		CASE(ELOOP);
		CASE(EMLINK);
		CASE(ENAMETOOLONG);
		CASE(ENOENT);
		CASE(ENOSPC);
		CASE(ENOTDIR);
		CASE(EROFS);
	case 0:
		return "success";
	default:
		return "???";
	}
}
