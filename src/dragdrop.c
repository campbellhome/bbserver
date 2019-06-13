// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "dragdrop.h"
#include "bbclient/bb_string.h"
#include "common.h"
#include "message_queue.h"
#include "recordings.h"
#include <shellapi.h>

#pragma comment(lib, "Shell32.lib")

void DragDrop_Init(void *hwnd)
{
	DragAcceptFiles(hwnd, 1);
}

void DragDrop_Shutdown(void)
{
}

static void DragDrop_ProcessPath(const char *path)
{
	BB_LOG("DragDrop", "Dropped file: %s", path);

	const char *separator = strrchr(path, '\\');
	if(separator) {
		const char *filename = separator + 1;
		if(*filename) {
			const char *ext = strrchr(filename, '.');
			if(ext && !bb_stricmp(ext, ".bbox")) {
				new_recording_t cmdlineRecording;
				GetSystemTimeAsFileTime(&cmdlineRecording.filetime);
				cmdlineRecording.applicationName = filename;
				cmdlineRecording.applicationFilename = filename;
				cmdlineRecording.path = path;
				cmdlineRecording.openView = true;
				cmdlineRecording.mainLog = false;
				cmdlineRecording.mqId = mq_invalid_id();
				cmdlineRecording.platform = kBBPlatform_Unknown;
				to_ui(kToUI_RecordingStart, "%s", recording_build_start_identifier(cmdlineRecording));
				//to_ui(kToUI_RecordingStop, "%s\n%s", filename, path);
			}
		}
	}
}

s32 DragDrop_OnDropFiles(u64 wparam)
{
	HDROP hdrop = (HDROP)wparam;
	char path[MAX_PATH];

	u32 numFiles = DragQueryFile(hdrop, ~0u, NULL, 0);
	for(u32 index = 0; index < numFiles; ++index) {
		if(DragQueryFile(hdrop, index, path, MAX_PATH) > 0) {
			DragDrop_ProcessPath(path);
		}
	}
	DragFinish(hdrop);
	return 1;
}
