// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "env_utils.h"

#if BB_USING(BB_PLATFORM_WINDOWS)
sb_t env_get(const char *name)
{
	DWORD required = GetEnvironmentVariable(name, NULL, 0);

	sb_t ret;
	sb_init(&ret);

	if(required) {
		if(sb_grow(&ret, required)) {
			GetEnvironmentVariable(name, ret.data, ret.count);
			ret.count = (u32)strlen(ret.data) + 1;
		}
	}

	return ret;
}

sb_t env_resolve(const char *src)
{
	DWORD required = ExpandEnvironmentStringsA(src, NULL, 0);

	sb_t ret;
	sb_init(&ret);

	if(required) {
		if(sb_grow(&ret, required)) {
			ExpandEnvironmentStringsA(src, ret.data, ret.count);
			ret.count = (u32)strlen(ret.data) + 1;
		}
	}

	return ret;
}
#endif // #if BB_USING(BB_PLATFORM_WINDOWS)

#if BB_USING(BB_PLATFORM_LINUX)

#include <stdlib.h>
#include <string.h>

sb_t env_get(const char *name)
{
	sb_t ret = {};
	char *var = getenv(name);
	if(var) {
		sb_append(&ret, var);
	}
	return ret;
}

sb_t env_resolve(const char *src)
{
	sb_t ret;
	sb_init(&ret);

	while(src) {
		const char *start = strchr(src, '%');
		if(start) {
			sb_append_range(&ret, src, start);
			const char *end = strchr(start + 1, '%');
			if(end) {
				sb_t name = {};
				sb_append_range(&name, start + 1, end);
				sb_t var = env_get(sb_get(&name));
				if(var.count > 1) {
					sb_append(&ret, sb_get(&var));
				} else {
					sb_append_range(&ret, start, end + 1);
				}
				sb_reset(&name);
				src = end + 1;
			} else {
				sb_append(&ret, start);
				break;
			}
		} else {
			sb_append(&ret, src);
			break;
		}
	}

	return ret;
}

#endif // #if BB_USING(BB_PLATFORM_LINUX)
