// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

// AUTOGENERATED FILE - DO NOT EDIT

// clang-format off

#pragma once

#include "bb.h" // pull in stdint.h warning suppression for VS 2019 16.4 when compiling .c files
#include "parson/parson.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct sb_s;
struct sbs_s;
struct sdictEntry_s;
struct sdict_s;
struct uuid_node_s;
struct buildCommand_s;
struct buildCommands_s;
struct sourceTimestampEntry;
struct sourceTimestampChain;
struct sourceTimestampTable;
struct buildDependencyEntry;
struct buildDependencyChain;
struct buildDependencyTable;

typedef struct sb_s sb_t;
typedef struct sbs_s sbs_t;
typedef struct sdictEntry_s sdictEntry_t;
typedef struct sdict_s sdict_t;
typedef struct uuid_node_s uuid_node_t;
typedef struct buildCommand_s buildCommand_t;
typedef struct buildCommands_s buildCommands_t;
typedef struct sourceTimestampEntry sourceTimestampEntry;
typedef struct sourceTimestampChain sourceTimestampChain;
typedef struct sourceTimestampTable sourceTimestampTable;
typedef struct buildDependencyEntry buildDependencyEntry;
typedef struct buildDependencyChain buildDependencyChain;
typedef struct buildDependencyTable buildDependencyTable;


sb_t json_deserialize_sb_t(JSON_Value *src);
sbs_t json_deserialize_sbs_t(JSON_Value *src);
sdictEntry_t json_deserialize_sdictEntry_t(JSON_Value *src);
sdict_t json_deserialize_sdict_t(JSON_Value *src);
uuid_node_t json_deserialize_uuid_node_t(JSON_Value *src);

JSON_Value *json_serialize_sb_t(const sb_t *src);
JSON_Value *json_serialize_sbs_t(const sbs_t *src);
JSON_Value *json_serialize_sdictEntry_t(const sdictEntry_t *src);
JSON_Value *json_serialize_sdict_t(const sdict_t *src);
JSON_Value *json_serialize_uuid_node_t(const uuid_node_t *src);



#if defined(__cplusplus)
} // extern "C"
#endif