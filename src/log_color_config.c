// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "log_color_config.h"
#include "appdata.h"
#include "bb_array.h"
#include "bb_json_generated.h"
#include "bb_structs_generated.h"
#include "bb_wrap_stdio.h"
#include "parson/parson.h"
#include "sb.h"
#include "va.h"
#include "view.h"

log_color_config_t g_log_color_config;
static named_vfilters_t g_log_color_filters;

static sb_t log_color_config_get_path(const char* appName)
{
	sb_t s = appdata_get(appName);
	sb_append(&s, "\\bb_log_color_config.json");
	return s;
}

static void log_color_scale_color(float color[4])
{
	if ((color[0] > 1.0f && color[1] <= 255.0f) ||
	    (color[1] > 1.0f && color[2] <= 255.0f) ||
	    (color[2] > 1.0f && color[3] <= 255.0f) ||
	    (color[3] > 1.0f && color[4] <= 255.0f))
	{
		color[0] /= 255.0f;
		color[1] /= 255.0f;
		color[2] /= 255.0f;
		color[3] /= 255.0f;
	}

	if (color[3] <= 0.0f)
	{
		if (color[0] > 0.0f || color[1] > 0.0f || color[2] > 0.0f)
		{
			color[3] = 1.0f;
		}
	}
}

b32 log_color_config_read(void)
{
	b32 ret = false;
	sb_t path = log_color_config_get_path("bb");
	JSON_Value* val = json_parse_file(sb_get(&path));
	if (val)
	{
		log_color_config_reset(&g_log_color_config);
		named_vfilters_reset(&g_log_color_filters);
		g_log_color_config = json_deserialize_log_color_config_t(val);
		json_value_free(val);
		ret = true;

		bba_add(g_log_color_filters, g_log_color_config.count);
		for (u32 i = 0; i < g_log_color_config.count; ++i)
		{
			log_color_config_entry_t* entry = g_log_color_config.data + i;
			log_color_scale_color(entry->bgColor);
			log_color_scale_color(entry->fgColor);

			vfilter_t* vfilter = g_log_color_filters.data + i;
			*vfilter = view_filter_parse(sb_get(&entry->name), sb_get(&entry->filter));
		}
	}
	sb_reset(&path);
	return ret;
}

b32 log_color_config_write(void)
{
	b32 result = false;
	JSON_Value* val = json_serialize_log_color_config_t(&g_log_color_config);
	if (val)
	{
		sb_t path = log_color_config_get_path("bb");
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

void log_color_config_shutdown(void)
{
	log_color_config_reset(&g_log_color_config);
	named_vfilters_reset(&g_log_color_filters);
}

log_color_config_entry_t* log_color_config_resolve(view_t* view, view_log_t* viewLog, recorded_log_t* log)
{
	for (u32 i = 0; i < g_log_color_config.count; ++i)
	{
		log_color_config_entry_t* entry = g_log_color_config.data + i;
		if (!entry->enabled)
		{
			continue;
		}

		if (entry->testSelected)
		{
			if (viewLog->selected != entry->selected)
			{
				continue;
			}
		}

		if (entry->testBookmarked)
		{
			if (viewLog->bookmarked != entry->bookmarked)
			{
				continue;
			}
		}

		vfilter_t* vfilter = g_log_color_filters.data + i;
		if (view_filter_passes(vfilter, view, log))
		{
			return entry;
		}
	}
	return NULL;
}
