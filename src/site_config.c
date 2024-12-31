// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "site_config.h"
#include "bb_json_generated.h"
#include "bb_structs_generated.h"

site_config_t g_site_config;

void site_config_init(void)
{
	JSON_Value* val = json_parse_file("bb_site_config.json");
	if (val)
	{
		g_site_config = json_deserialize_site_config_t(val);
		json_value_free(val);
	}
	if (!g_site_config.bugPort)
	{
		g_site_config.bugPort = 51984;
	}
}

void site_config_shutdown(void)
{
	site_config_reset(&g_site_config);
}
