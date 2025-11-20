// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "bbserver_utils.h"
#include "sb.h"
#include "span.h"

#include "parson/parson.h"

#include "bb_wrap_windows.h"

void BBServer_OpenDirInExplorer(const char* dir)
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
	if (!ret)
	{
		BB_ERROR("View", "Failed to create process for '%s'", sb.data);
	}
	else
	{
		BB_LOG("View", "Created process for '%s'", sb.data);
	}
	CloseHandle(procInfo.hThread);
	CloseHandle(procInfo.hProcess);

	sb_reset(&sb);
}

sb_t sb_expand_json(sb_t source)
{
	sb_t target = { BB_EMPTY_INITIALIZER };
	JSON_Value* val = json_parse_string(sb_get(&source));
	if (val)
	{
		char* json = json_serialize_to_string_pretty(val);
		if (json)
		{
			sb_append(&target, json);
			json_free_serialized_string(json);
		}
		json_value_free(val);
	}
	return target;
}

void span_strip_color_codes(span_t span, sb_t* output)
{
	output->count = 0;
	if (output->data)
	{
		output->data[0] = '\0';
	}
	const char* text = span.start;
	if (text)
	{
		while (*text && text < span.end)
		{
			if (*text == kColorKeyPrefix && text[1] >= kFirstColorKey && text[1] <= kLastColorKey)
			{
				++text;
			}
			else if (text[0] == '^' && text[1] == 'F')
			{
				++text;
			}
			else
			{
				sb_append_char(output, *text);
			}
			++text;
		}
	}
}

// TODO: consolidate ui_view.cpp line_can_be_json with this one
b32 line_can_be_json(sb_t line)
{
	b32 result = false;
	if (line.data && line.count > 2)
	{
		if (line.data[0] == '{' && line.data[line.count - 2] == '}')
		{
			for (u32 index = 1; index < line.count - 2; ++index)
			{
				if (line.data[index] == ' ' || line.data[index] == '\t')
					continue;
				if (line.data[index] == '\"')
				{
					result = true;
				}
				break;
			}
		}
		else if (line.data[0] == '[' && line.data[line.count - 2] == ']')
		{
			for (u32 index = 1; index < line.count - 2; ++index)
			{
				if (line.data[index] == ' ' || line.data[index] == '\t')
					continue;
				if (line.data[index] == '\"' || line.data[index] == '{')
				{
					result = true;
				}
				break;
			}
		}
	}
	return result;
}
