// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "uuid_config.h"
#include "appdata.h"
#include "bb_json_generated.h"
#include "bb_structs_generated.h"
#include "bb_wrap_stdio.h"
#include "sb.h"

static sb_t uuid_get_path(const char* appName)
{
	sb_t s = appdata_get(appName);
	sb_append(&s, "\\uuid_config.json");
	return s;
}

void uuid_read_state(u16* clockSequence, u32* timestampLow, u32* timestampHi, uuid_node_t* nodeId)
{
	sb_t path = uuid_get_path("bb");
	JSON_Value* val = json_parse_file(sb_get(&path));
	if (val)
	{
		uuidState_t uuidState = json_deserialize_uuidState_t(val);
		json_value_free(val);
		*clockSequence = uuidState.clockSequence;
		*timestampHi = uuidState.timestampHi;
		*timestampLow = uuidState.timestampLow;
		*nodeId = uuidState.nodeId;
	}
	else
	{
		*clockSequence = 0;
		*timestampHi = 0;
		*timestampLow = 0;
		*nodeId = generate_uuid_node_identifier();
	}
	sb_reset(&path);
}

void uuid_write_state(u16 clockSequence, u32 timestampLow, u32 timestampHi, uuid_node_t nodeId)
{
	uuidState_t uuidState;
	uuidState.timestampHi = timestampHi;
	uuidState.timestampLow = timestampLow;
	uuidState.clockSequence = clockSequence;
	uuidState.nodeId = nodeId;

	JSON_Value* val = json_serialize_uuidState_t(&uuidState);
	if (val)
	{
		sb_t path = uuid_get_path("bb");
		FILE* fp = fopen(sb_get(&path), "wb");
		if (fp)
		{
			char* serialized_string = json_serialize_to_string_pretty(val);
			fputs(serialized_string, fp);
			fclose(fp);
			json_free_serialized_string(serialized_string);
		}
		sb_reset(&path);
	}
	json_value_free(val);
}
