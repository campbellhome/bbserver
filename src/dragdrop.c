// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "dragdrop.h"
#include "bb_packet.h"
#include "bb_string.h"
#include "bb_structs_generated.h"
#include "common.h"
#include "file_utils.h"
#include "message_queue.h"
#include "recordings.h"
#include "va.h"
#include "view.h"
#include <shellapi.h>

#pragma comment(lib, "Shell32.lib")

void sanitize_app_filename(const char* applicationName, char* applicationFilename, size_t applicationFilenameLen);

void DragDrop_Init(void* hwnd)
{
	DragAcceptFiles(hwnd, 1);
}

void DragDrop_Shutdown(void)
{
}

void DragDrop_ProcessPath(const char* path)
{
	BB_LOG("DragDrop", "Dropped file: %s", path);

	while (*path == ' ' || *path == '\t')
	{
		++path;
	}
	sb_t pathStr = sb_from_c_string(path);
	path = sb_get(&pathStr);
	while (pathStr.count > 2)
	{
		if (pathStr.data[pathStr.count - 2] == ' ' || pathStr.data[pathStr.count - 2] == '\t')
		{
			--pathStr.count;
			pathStr.data[pathStr.count - 1] = '\0';
		}
		else
		{
			break;
		}
	}

	const char* separator = strrchr(path, '\\');
	if (separator)
	{
		const char* filename = separator + 1;
		if (*filename)
		{
			const char* ext = strrchr(filename, '.');
			if (ext && !bb_stricmp(ext, ".bbox"))
			{
				bb_decoded_packet_t decoded = { BB_EMPTY_INITIALIZER };
				b32 valid = recordings_get_application_info(path, &decoded);
				if (valid)
				{
					char applicationFilename[kBBSize_ApplicationName];
					new_recording_t cmdlineRecording = { BB_EMPTY_INITIALIZER };
					GetSystemTimeAsFileTime(&cmdlineRecording.filetime);
					FILETIME creationTime, accessTime, lastWriteTime;
					if (file_getTimestamps(path, &creationTime, &accessTime, &lastWriteTime))
					{
						cmdlineRecording.filetime = CompareFileTime(&lastWriteTime, &creationTime) >= 0 ? lastWriteTime : creationTime;
					}
					cmdlineRecording.applicationName = sb_from_c_string(decoded.packet.appInfo.applicationName);
					sanitize_app_filename(sb_get(&cmdlineRecording.applicationName), applicationFilename, sizeof(applicationFilename));
					cmdlineRecording.applicationFilename = sb_from_c_string(applicationFilename);
					cmdlineRecording.path = sb_from_c_string(path);
					cmdlineRecording.openView = true;
					cmdlineRecording.recordingType = kRecordingType_ExternalFile;
					cmdlineRecording.mqId = mq_invalid_id();
					cmdlineRecording.platform = decoded.packet.appInfo.platform;
					to_ui(kToUI_RecordingStart, "%s", recording_build_start_identifier(cmdlineRecording));
					new_recording_reset(&cmdlineRecording);
				}
				else
				{
					BB_WARNING("DragDrop", "Failed to find application info for %s", path);
				}
			}
			else
			{
				if (!ext || strlen(ext) <= 1)
				{
					ext = ".unknown";
				}
				char applicationFilename[kBBSize_ApplicationName];
				new_recording_t cmdlineRecording = { BB_EMPTY_INITIALIZER };
				GetSystemTimeAsFileTime(&cmdlineRecording.filetime);
				FILETIME creationTime, accessTime, lastWriteTime;
				if (file_getTimestamps(path, &creationTime, &accessTime, &lastWriteTime))
				{
					cmdlineRecording.filetime = lastWriteTime;
				}
				cmdlineRecording.applicationName = sb_from_c_string(va("%s file", ext + 1));
				sanitize_app_filename(sb_get(&cmdlineRecording.applicationName), applicationFilename, sizeof(applicationFilename));
				cmdlineRecording.applicationFilename = sb_from_c_string(applicationFilename);
				cmdlineRecording.path = sb_from_c_string(path);
				cmdlineRecording.openView = true;
				cmdlineRecording.recordingType = kRecordingType_ExternalFile;
				cmdlineRecording.mqId = mq_invalid_id();
				cmdlineRecording.platform = bb_platform();
				to_ui(kToUI_RecordingStart, "%s", recording_build_start_identifier(cmdlineRecording));
				new_recording_reset(&cmdlineRecording);
			}
		}
	}
	sb_reset(&pathStr);
}

s32 DragDrop_OnDropFiles(u64 wparam)
{
	HDROP hdrop = (HDROP)wparam;
	char path[MAX_PATH];

	u32 numFiles = DragQueryFile(hdrop, ~0u, NULL, 0);
	for (u32 index = 0; index < numFiles; ++index)
	{
		if (DragQueryFile(hdrop, index, path, MAX_PATH) > 0)
		{
			DragDrop_ProcessPath(path);
		}
	}
	DragFinish(hdrop);
	return 1;
}
