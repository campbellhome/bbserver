// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"
#include "sb.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct updateVersionName_s
{
	sb_t name;
} updateVersionName_t;

typedef struct updateVersion_s
{
	sb_t name;
	sb_t path;
} updateVersion_t;

typedef struct updateVersionList_s
{
	u32 count;
	u32 allocated;
	updateVersion_t* data;
} updateVersionList_t;

typedef struct updateManifest_s
{
	updateVersionList_t versions;
	sb_t stable;
	sb_t latest;
} updateManifest_t;

sb_t update_get_archive_name(const char* rootDir, const char* appName, const char* name);
const char* update_resolve_version(const updateManifest_t* manifest, const char* versionName);
updateManifest_t updateManifest_build(const char* updateManifestDir, const char* appName);

void updateVersionName_reset(updateVersionName_t* val);
void updateVersion_reset(updateVersion_t* val);
void updateVersionList_reset(updateVersionList_t* val);
void updateManifest_reset(updateManifest_t* val);

updateVersionName_t updateVersionName_clone(updateVersionName_t* src);
updateVersion_t updateVersion_clone(updateVersion_t* src);
updateVersionList_t updateVersionList_clone(updateVersionList_t* src);
updateManifest_t updateManifest_clone(updateManifest_t* src);

typedef struct json_value_t JSON_Value;

updateVersionName_t json_deserialize_updateVersionName_t(JSON_Value* src);
updateVersion_t json_deserialize_updateVersion_t(JSON_Value* src);
updateVersionList_t json_deserialize_updateVersionList_t(JSON_Value* src);
updateManifest_t json_deserialize_updateManifest_t(JSON_Value* src);

JSON_Value* json_serialize_updateVersionName_t(const updateVersionName_t* src);
JSON_Value* json_serialize_updateVersion_t(const updateVersion_t* src);
JSON_Value* json_serialize_updateVersionList_t(const updateVersionList_t* src);
JSON_Value* json_serialize_updateManifest_t(const updateManifest_t* src);

#if defined(__cplusplus)
}
#endif
