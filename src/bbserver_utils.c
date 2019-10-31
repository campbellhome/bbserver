// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "bbserver_utils.h"
#include "sb.h"
BB_WARNING_PUSH(4577 4668 4820 4946)
#include <windows.h> // For common windows data types and function headers
BB_WARNING_POP

void BBServer_OpenDirInExplorer(const char *dir)
{
	sb_t sb;
	sb_init(&sb);
	sb_append(&sb, "C:\\Windows\\explorer.exe \"");
	sb_append(&sb, dir);
	sb_append(&sb, "\"");

	STARTUPINFOA startupInfo;
	memset(&startupInfo, 0, sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);
	PROCESS_INFORMATION procInfo;
	memset(&procInfo, 0, sizeof(procInfo));
	BOOL ret = CreateProcessA(NULL, sb.data, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &startupInfo, &procInfo);
	if(!ret) {
		BB_ERROR("View", "Failed to create process for '%s'", sb.data);
	} else {
		BB_LOG("View", "Created process for '%s'", sb.data);
	}
	CloseHandle(procInfo.hThread);
	CloseHandle(procInfo.hProcess);

	sb_reset(&sb);
}
