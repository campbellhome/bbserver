// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "recordings_config.h"
#include "bb_string.h"
#include "bb_wrap_stdio.h"
#include "file_utils.h"
#include "line_parser.h"
#include "recordings.h"
#include "sb.h"
#include "va.h"
#include <stdlib.h>

static const char *s_recordingsSortNames[] = {
	"StartTime",
	"Application",
};
BB_CTASSERT(BB_ARRAYSIZE(s_recordingsSortNames) == kRecordingSort_Count);
static recording_sort_t s_recordingsDefaultSort = kRecordingSort_StartTime;

static const char *s_recordingsGroupNames[] = {
	"None",
	"Application",
};
BB_CTASSERT(BB_ARRAYSIZE(s_recordingsGroupNames) == kRecordingGroup_Count);
static recording_group_t s_recordingsDefaultGroup = kRecordingGroup_Application;

void get_appdata_folder(char *buffer, size_t bufferSize);

static b32 recordings_get_config_path(char *buffer, size_t bufferSize)
{
	size_t dirLen;
	get_appdata_folder(buffer, bufferSize);
	dirLen = strlen(buffer);
	if(bb_snprintf(buffer + dirLen, bufferSize - dirLen, "\\bb_recordings.bbconfig") < 0)
		return false;
	return true;
}

b32 recordings_write_config(recordings_t *recordings)
{
	char path[kBBSize_MaxPath];
	FILE *fp;
	if(!recordings_get_config_path(path, sizeof(path)))
		return false;

	fp = fopen(path, "wb");
	if(!fp)
		return false;

	fprintf(fp, "[1] # Version - do not remove\n");
	fprintf(fp, "\n");
	if(recordings->sort != s_recordingsDefaultSort) {
		fprintf(fp, "sort \"%s\"\n", s_recordingsSortNames[recordings->sort]);
	}
	if(recordings->group != s_recordingsDefaultGroup) {
		fprintf(fp, "group \"%s\"\n", s_recordingsGroupNames[recordings->group]);
	}
	fprintf(fp, "showDate %d\n", recordings->showDate);
	fprintf(fp, "showTime %d\n", recordings->showTime);
	fprintf(fp, "showInternal %d\n", recordings->showInternal);
	fprintf(fp, "showExternal %d\n", recordings->showExternal);
	fprintf(fp, "width %f\n", recordings->width);
	fclose(fp);
	return true;
}

static b32 recordings_parse_sort(line_parser_t *parser, recordings_t *recordings)
{
	b32 ret = true;
	u32 i;
	char *token = line_parser_next_token(parser);
	if(token) {
		b32 found = false;
		for(i = 0; i < kRecordingSort_Count; ++i) {
			if(!strcmp(s_recordingsSortNames[i], token)) {
				recordings->sort = (recording_sort_t)i;
				found = true;
				break;
			}
		}
		if(!found) {
			ret = line_parser_error(parser, va("invalid sort value '%s'", token));
		}
	} else {
		ret = line_parser_error(parser, "missing sort value");
	}
	return ret;
}

static b32 recordings_parse_group(line_parser_t *parser, recordings_t *recordings)
{
	b32 ret = true;
	u32 i;
	char *token = line_parser_next_token(parser);
	if(token) {
		b32 found = false;
		for(i = 0; i < kRecordingGroup_Count; ++i) {
			if(!strcmp(s_recordingsGroupNames[i], token)) {
				recordings->group = (recording_group_t)i;
				found = true;
				break;
			}
		}
		if(!found) {
			ret = line_parser_error(parser, va("invalid group value '%s'", token));
		}
	} else {
		ret = line_parser_error(parser, "missing group value");
	}
	return ret;
}

static b32 recordings_parse_b32(line_parser_t *parser, b32 *target, const char *name)
{
	b32 ret = true;
	char *token = line_parser_next_token(parser);
	if(token) {
		*target = atoi(token) != 0;
	} else {
		ret = line_parser_error(parser, va("missing %s value", name));
	}
	return ret;
}

static b32 recordings_parse_float(line_parser_t *parser, float *target, const char *name)
{
	b32 ret = true;
	char *token = line_parser_next_token(parser);
	if(token) {
		*target = (float)atof(token);
	} else {
		ret = line_parser_error(parser, va("missing %s value", name));
	}
	return ret;
}

static b32 recordings_read_config_lines(line_parser_t *parser, recordings_t *recordings)
{
	char *line;
	char *token;
	b32 ret = true;
	while((line = line_parser_next_line(parser)) != NULL) {
		//BB_LOG("Parser", "Line %u: [%s]\n", parser->lineIndex, line);
		token = line_parser_next_token(parser);
		if(!strcmp(token, "sort")) {
			ret = recordings_parse_sort(parser, recordings) || ret;
		} else if(!strcmp(token, "group")) {
			ret = recordings_parse_group(parser, recordings) || ret;
		} else if(!strcmp(token, "showDate")) {
			ret = recordings_parse_b32(parser, &recordings->showDate, "showDate") || ret;
		} else if(!strcmp(token, "showTime")) {
			ret = recordings_parse_b32(parser, &recordings->showTime, "showTime") || ret;
		} else if(!strcmp(token, "showInternal")) {
			ret = recordings_parse_b32(parser, &recordings->showInternal, "showInternal") || ret;
		} else if(!strcmp(token, "showExternal")) {
			ret = recordings_parse_b32(parser, &recordings->showExternal, "showExternal") || ret;
		} else if(!strcmp(token, "width")) {
			ret = recordings_parse_float(parser, &recordings->width, "showTime") || ret;
		} else {
			ret = line_parser_error(parser, va("unknown token '%s'", token));
		}
	}
	return 0;
}

static void recordings_default_config(recordings_t *recordings)
{
	recordings->sort = s_recordingsDefaultSort;
	recordings->group = s_recordingsDefaultGroup;
	recordings->width = 275.0f;
	recordings->showDate = recordings->showTime = true;
	recordings->showInternal = recordings->showExternal = true;
}

b32 recordings_read_config(recordings_t *recordings)
{
	char path[kBBSize_MaxPath];
	fileData_t fileData;
	line_parser_t parser;
	u32 version;
	b32 result;
	recordings_default_config(recordings);
	if(!recordings_get_config_path(path, sizeof(path)))
		return false;

	fileData = fileData_read(path);
	if(!fileData.buffer)
		return false;

	line_parser_init(&parser, path, (char *)fileData.buffer);
	version = line_parser_read_version(&parser);
	if(!version)
		return false;
	if(version > 1) {
		return line_parser_error(&parser, va("expected version 1, saw [%u]", version));
	}
	result = recordings_read_config_lines(&parser, recordings);
	fileData_reset(&fileData);
	if(recordings->width == 0.0f) {
		recordings_default_config(recordings);
	}
	return result;
}
