// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "appdata.h"
#include "path_utils.h"

#if BB_USING(BB_PLATFORM_WINDOWS)
// warning C4820 : 'StructName' : '4' bytes padding added after data member 'MemberName'
// warning C4255: 'FuncName': no function prototype given: converting '()' to '(void)'
BB_WARNING_PUSH(4820 4255)
#include "bb_wrap_windows.h"
#include <ShlObj.h>
#pragma comment(lib, "Shell32.lib")
sb_t appdata_get(const char *appName)
{
	sb_t ret;
	sb_init(&ret);

	//size_t len;
	char appData[_MAX_PATH] = "C:";
	if(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appData) == S_OK) {
		sb_append(&ret, appData);
	}
	sb_append(&ret, "\\");
	sb_append(&ret, appName);
	path_mkdir(sb_get(&ret));
	return ret;
}
BB_WARNING_POP;
#else
#include "env_utils.h"
#include "sb.h"
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
sb_t appdata_get(const char *appName)
{
	sb_t ret = {};
	char temp[1024] = "~";
	struct passwd pwd;
	sb_t home = env_get("HOME");
	if(!sb_len(&home)) {
		struct passwd *ppwd = NULL;
		getpwuid_r(getuid(), &pwd, temp, sizeof(temp), &ppwd);
		if(ppwd) {
			sb_append(&home, ppwd->pw_dir);
		} else {
			sb_append(&home, "~");
		}
	}

	sb_va(&ret, "%s/.config/%s", sb_get(&home), appName);
	path_mkdir(sb_get(&ret));
	sb_reset(&home);
	return ret;
}
#endif
