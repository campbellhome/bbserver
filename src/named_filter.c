// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "named_filter.h"
#include "appdata.h"
#include "bb_array.h"
#include "bb_json_generated.h"
#include "bb_structs_generated.h"
#include "bb_wrap_stdio.h"
#include "parson/parson.h"
#include "sb.h"
#include "va.h"
#include "view.h"

named_filters_t g_user_named_filters;
static named_vfilters_t g_user_named_vfilters;

static sb_t log_color_config_get_path(const char* appName)
{
	sb_t s = appdata_get(appName);
	sb_append(&s, "\\bb_named_filters_config.json");
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

b32 named_filters_read(void)
{
	b32 ret = false;
	b32 configUpdate = false;
	sb_t path = log_color_config_get_path("bb");
	JSON_Value* val = json_parse_file(sb_get(&path));
	if (val)
	{
		named_filters_reset(&g_user_named_filters);
		named_vfilters_reset(&g_user_named_vfilters);
		g_user_named_filters = json_deserialize_named_filters_t(val);
		json_value_free(val);
		ret = true;

		// read old bb_config.json named filters
		sb_t configPath = config_get_path("bb");
		JSON_Value* configVal = json_parse_file(sb_get(&configPath));
		if (configVal)
		{
			JSON_Object* configObj = json_value_get_object(configVal);
			if (configObj)
			{
				JSON_Array* configNamedFiltersArr = json_object_get_array(configObj, "namedFilters");
				if (configNamedFiltersArr)
				{
					if (configNamedFiltersArr)
					{
						for (u32 i = 0; i < json_array_get_count(configNamedFiltersArr); ++i)
						{
							configUpdate = true;
							bba_push(g_user_named_filters, json_deserialize_named_filter_t(json_array_get_value(configNamedFiltersArr, i)));
						}
					}
				}
			}
			json_value_free(configVal);
		}
		sb_reset(&configPath);

		bba_add(g_user_named_vfilters, g_user_named_filters.count);
		for (u32 i = 0; i < g_user_named_filters.count; ++i)
		{
			named_filter_t* entry = g_user_named_filters.data + i;
			log_color_scale_color(entry->bgColor);
			log_color_scale_color(entry->bgColorActive);
			log_color_scale_color(entry->bgColorHovered);
			log_color_scale_color(entry->fgColor);

			vfilter_t* vfilter = g_user_named_vfilters.data + i;
			*vfilter = view_filter_parse(sb_get(&entry->name), sb_get(&entry->text));
		}
	}
	sb_reset(&path);

	if (configUpdate)
	{
		config_write(&g_config);
		named_filters_write();
	}

	return ret;
}

b32 named_filters_write(void)
{
	b32 result = false;
	JSON_Value* val = json_serialize_named_filters_t(&g_user_named_filters);
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

void named_filters_rebuild(void)
{
	named_filters_write();
	named_filters_shutdown();
	named_filters_read();
}

void named_filters_shutdown(void)
{
	named_filters_reset(&g_user_named_filters);
	named_vfilters_reset(&g_user_named_vfilters);
}

named_filter_t* named_filters_resolve(view_t* view, view_log_t* viewLog, recorded_log_t* log, b32 customColors)
{
	for (u32 i = 0; i < g_user_named_filters.count; ++i)
	{
		named_filter_t* entry = g_user_named_filters.data + i;
		if (!entry->filterEnabled)
		{
			continue;
		}

		if (customColors && !entry->customColorsEnabled)
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

		vfilter_t* vfilter = g_user_named_vfilters.data + i;
		if (view_filter_passes(vfilter, view, log))
		{
			return entry;
		}
	}
	return NULL;
}
