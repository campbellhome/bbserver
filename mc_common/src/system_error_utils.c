// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "sb.h"

#if BB_USING(BB_PLATFORM_WINDOWS)

sb_t system_error_to_sb_from_loc(const char *file, int line, u32 err)
{
	char *errorMessage = NULL;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&errorMessage, 0, NULL);
	sb_t ret = sb_from_c_string_from_loc(file, line, errorMessage ? errorMessage : "unable to format error message");
	if(errorMessage) {
		LocalFree(errorMessage);
	}
	return ret;
}

#endif // #if BB_USING(BB_PLATFORM_WINDOWS)

#if BB_USING(BB_PLATFORM_LINUX)

#include <errno.h>

sb_t system_error_to_sb_from_loc(const char *file, int line, u32 err)
{
	char *errorMessage = NULL;
	switch(err) {
#define CASE(x) \
	case x:     \
		errorMessage = #x
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
		errorMessage = "success";
	default:
		break;
	}

	return sb_from_c_string_from_loc(file, line, errorMessage ? errorMessage : "unable to format error message");
}

#endif // #if BB_USING(BB_PLATFORM_LINUX)

#if BB_USING(BB_PLATFORM_WINDOWS) || BB_USING(BB_PLATFORM_LINUX)
void system_error_to_log_from_loc(const char *file, int line, u32 err, const char *category, const char *text)
{
	sb_t msg = system_error_to_sb_from_loc(file, line, err);
	BB_ERROR_DYNAMIC(file, line, category, "%s:\n  Error %u (0x%8.8X): %s", text, err, err, sb_get(&msg));
	sb_reset(&msg);
}
#endif // #if BB_USING(BB_PLATFORM_WINDOWS) || BB_USING(BB_PLATFORM_LINUX)
