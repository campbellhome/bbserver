// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "recordings_config.h"
#include "appdata.h"
#include "bb_json_generated.h"
#include "bb_string.h"
#include "bb_wrap_stdio.h"
#include "file_utils.h"
#include "line_parser.h"
#include "parson/parson.h"
#include "recordings.h"
#include "sb.h"
#include "va.h"
#include <stdlib.h>

static const char* s_recordingsSortNames[] = {
	"StartTime",
	"Application",
};
BB_CTASSERT(BB_ARRAYSIZE(s_recordingsSortNames) == kRecordingSort_Count);
static recording_sort_t s_recordingsDefaultSort = kRecordingSort_StartTime;

static const char* s_recordingsGroupNames[] = {
	"None",
	"Application",
};
BB_CTASSERT(BB_ARRAYSIZE(s_recordingsGroupNames) == kRecordingGroup_Count);
static recording_group_t s_recordingsDefaultGroup = kRecordingGroup_Application;

void get_appdata_folder(char* buffer, size_t bufferSize);

static b32 recordings_get_config_path(char* buffer, size_t bufferSize)
{
	size_t dirLen;
	get_appdata_folder(buffer, bufferSize);
	dirLen = strlen(buffer);
	if (bb_snprintf(buffer + dirLen, bufferSize - dirLen, "\\bb_recordings.bbconfig") < 0)
		return false;
	return true;
}

static b32 recordings_parse_sort(line_parser_t* parser, recordings_config_t* config)
{
	b32 ret = true;
	u32 i;
	char* token = line_parser_next_token(parser);
	if (token)
	{
		b32 found = false;
		for (i = 0; i < kRecordingSort_Count; ++i)
		{
			if (!strcmp(s_recordingsSortNames[i], token))
			{
				config->tabs[0].sort = (recording_sort_t)i;
				found = true;
				break;
			}
		}
		if (!found)
		{
			ret = line_parser_error(parser, va("invalid sort value '%s'", token));
		}
	}
	else
	{
		ret = line_parser_error(parser, "missing sort value");
	}
	return ret;
}

static b32 recordings_parse_group(line_parser_t* parser, recordings_config_t* config)
{
	b32 ret = true;
	u32 i;
	char* token = line_parser_next_token(parser);
	if (token)
	{
		b32 found = false;
		for (i = 0; i < kRecordingGroup_Count; ++i)
		{
			if (!strcmp(s_recordingsGroupNames[i], token))
			{
				config->tabs[0].group = (recording_group_t)i;
				found = true;
				break;
			}
		}
		if (!found)
		{
			ret = line_parser_error(parser, va("invalid group value '%s'", token));
		}
	}
	else
	{
		ret = line_parser_error(parser, "missing group value");
	}
	return ret;
}

static b32 recordings_parse_b32(line_parser_t* parser, b32* target, const char* name)
{
	b32 ret = true;
	char* token = line_parser_next_token(parser);
	if (token)
	{
		*target = atoi(token) != 0;
	}
	else
	{
		ret = line_parser_error(parser, va("missing %s value", name));
	}
	return ret;
}

static b32 recordings_parse_float(line_parser_t* parser, float* target, const char* name)
{
	b32 ret = true;
	char* token = line_parser_next_token(parser);
	if (token)
	{
		*target = (float)atof(token);
	}
	else
	{
		ret = line_parser_error(parser, va("missing %s value", name));
	}
	return ret;
}

static b32 recordings_read_config_lines(line_parser_t* parser, recordings_config_t* config)
{
	char* line;
	char* token;
	b32 ret = true;
	b32 dummy = false;
	while ((line = line_parser_next_line(parser)) != NULL)
	{
		//BB_LOG("Parser", "Line %u: [%s]\n", parser->lineIndex, line);
		token = line_parser_next_token(parser);
		if (!strcmp(token, "sort"))
		{
			ret = recordings_parse_sort(parser, config) || ret;
		}
		else if (!strcmp(token, "group"))
		{
			ret = recordings_parse_group(parser, config) || ret;
		}
		else if (!strcmp(token, "showDate"))
		{
			ret = recordings_parse_b32(parser, &config->tabs[0].showDate, "showDate") || ret;
		}
		else if (!strcmp(token, "showTime"))
		{
			ret = recordings_parse_b32(parser, &config->tabs[0].showTime, "showTime") || ret;
		}
		else if (!strcmp(token, "showInternal"))
		{
			ret = recordings_parse_b32(parser, &dummy, "showInternal") || ret;
		}
		else if (!strcmp(token, "showExternal"))
		{
			ret = recordings_parse_b32(parser, &dummy, "showExternal") || ret;
		}
		else if (!strcmp(token, "width"))
		{
			ret = recordings_parse_float(parser, &config->width, "showTime") || ret;
		}
		else
		{
			ret = line_parser_error(parser, va("unknown token '%s'", token));
		}
	}
	return ret;
}

static void recordings_default_config(recordings_config_t* config)
{
	config->tabs[0].sort = s_recordingsDefaultSort;
	config->tabs[0].group = s_recordingsDefaultGroup;
	config->tabs[0].showDate = config->tabs[0].showTime = true;
	for (u32 tab = 1; tab < kRecordingTab_Count; ++tab)
	{
		config->tabs[tab] = config->tabs[0];
	}
	config->width = 275.0f;
	config->recordingsOpen = true;
}

static b32 recordings_read_config_old(recordings_config_t* config)
{
	char path[kBBSize_MaxPath];
	fileData_t fileData;
	line_parser_t parser;
	u32 version;
	b32 result;
	recordings_default_config(config);
	if (!recordings_get_config_path(path, sizeof(path)))
		return false;

	fileData = fileData_read(path);
	if (!fileData.buffer)
		return false;

	line_parser_init(&parser, path, (char*)fileData.buffer);
	version = line_parser_read_version(&parser);
	if (!version)
		return false;
	if (version > 1)
	{
		return line_parser_error(&parser, va("expected version 1, saw [%u]", version));
	}
	result = recordings_read_config_lines(&parser, config);
	fileData_reset(&fileData);
	if (config->width == 0.0f)
	{
		recordings_default_config(config);
	}
	for (u32 tab = 1; tab < kRecordingTab_Count; ++tab)
	{
		config->tabs[tab] = config->tabs[0];
	}
	return result;
}

static sb_t recordings_config_get_path(const char* appName)
{
	sb_t s = appdata_get(appName);
	sb_append(&s, "\\bb_recordings_config.json");
	return s;
}

b32 recordings_config_read(recordings_config_t* config)
{
	b32 ret = false;
	sb_t path = recordings_config_get_path("bb");
	JSON_Value* val = json_parse_file(sb_get(&path));
	if (val)
	{
		*config = json_deserialize_recordings_config_t(val);
		json_value_free(val);
		ret = true;
	}

	if (!ret)
	{
		ret = recordings_read_config_old(config);
	}
	sb_reset(&path);
	return ret;
}

b32 recordings_config_write(recordings_config_t* config)
{
	b32 result = false;
	JSON_Value* val = json_serialize_recordings_config_t(config);
	if (val)
	{
		sb_t path = recordings_config_get_path("bb");
		FILE* fp = fopen(sb_get(&path), "wb");
		if (fp)
		{
			char* serialized_string = json_serialize_to_string_pretty(val);
			fputs(serialized_string, fp);
			fclose(fp);
			json_free_serialized_string(serialized_string);
			result = true;
		}
		sb_reset(&path);
	}
	json_value_free(val);
	return result;
}
