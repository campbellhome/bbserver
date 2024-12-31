// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "update_utils.h"
#include "bb_array.h"
#include "bb_file.h"
#include "bb_wrap_dirent.h"
#include "parson/parson.h"
#include "path_utils.h"
#include <stdlib.h>

static int update_version_compare(const void* _a, const void* _b)
{
	const updateVersion_t* a = _a;
	const updateVersion_t* b = _b;
	int aver = atoi(sb_get(&a->name));
	int bver = atoi(sb_get(&b->name));
	int result = bver - aver;
	return result;
}

sb_t update_get_archive_name(const char* rootDir, const char* appName, const char* name)
{
	sb_t path = { BB_EMPTY_INITIALIZER };
	sb_va(&path, "%s/%s/%s_%s.zip", rootDir, name, appName, name);
	path_resolve_inplace(&path);
	return path;
}

const char* update_resolve_version(const updateManifest_t* manifest, const char* versionName)
{
	if (versionName && !strcmp(versionName, "latest"))
	{
		versionName = sb_get(&manifest->latest);
	}
	else if (versionName && !strcmp(versionName, "stable"))
	{
		versionName = sb_get(&manifest->stable);
	}
	return versionName;
}

updateManifest_t updateManifest_build(const char* updateManifestDir, const char* appName)
{
	updateManifest_t manifest = { BB_EMPTY_INITIALIZER };
	DIR* d = opendir(updateManifestDir);
	if (d)
	{
		struct dirent* dir;
		while ((dir = readdir(d)) != NULL)
		{
			if (dir->d_type == DT_DIR)
			{
				if (strcmp(dir->d_name, ".") && strcmp(dir->d_name, ".."))
				{
					sb_t archivePath = update_get_archive_name(updateManifestDir, appName, dir->d_name);
					const char* archiveName = sb_get(&archivePath);
					if (bb_file_readable(archiveName))
					{
						updateVersion_t update = { BB_EMPTY_INITIALIZER };
						sb_append(&update.name, dir->d_name);
						sb_append(&update.path, archiveName);
						bba_push(manifest.versions, update);
					}
					sb_reset(&archivePath);
				}
			}
		}
		closedir(d);
		qsort(manifest.versions.data, manifest.versions.count, sizeof(updateVersion_t), &update_version_compare);
		if (manifest.versions.data)
		{
			sb_append(&manifest.latest, sb_get(&manifest.versions.data->name));
		}
	}
	return manifest;
}

void updateVersionName_reset(updateVersionName_t* val)
{
	if (val)
	{
		sb_reset(&val->name);
	}
}
updateVersionName_t updateVersionName_clone(updateVersionName_t* src)
{
	updateVersionName_t dst = { BB_EMPTY_INITIALIZER };
	if (src)
	{
		dst.name = sb_clone(&src->name);
	}
	return dst;
}

void updateVersion_reset(updateVersion_t* val)
{
	if (val)
	{
		sb_reset(&val->name);
		sb_reset(&val->path);
	}
}
updateVersion_t updateVersion_clone(updateVersion_t* src)
{
	updateVersion_t dst = { BB_EMPTY_INITIALIZER };
	if (src)
	{
		dst.name = sb_clone(&src->name);
		dst.path = sb_clone(&src->path);
	}
	return dst;
}

void updateVersionList_reset(updateVersionList_t* val)
{
	if (val)
	{
		for (u32 i = 0; i < val->count; ++i)
		{
			updateVersion_reset(val->data + i);
		}
		bba_free(*val);
	}
}
updateVersionList_t updateVersionList_clone(updateVersionList_t* src)
{
	updateVersionList_t dst = { BB_EMPTY_INITIALIZER };
	if (src)
	{
		for (u32 i = 0; i < src->count; ++i)
		{
			if (bba_add_noclear(dst, 1))
			{
				bba_last(dst) = updateVersion_clone(src->data + i);
			}
		}
	}
	return dst;
}

void updateManifest_reset(updateManifest_t* val)
{
	if (val)
	{
		updateVersionList_reset(&val->versions);
		sb_reset(&val->stable);
		sb_reset(&val->latest);
	}
}
updateManifest_t updateManifest_clone(updateManifest_t* src)
{
	updateManifest_t dst = { BB_EMPTY_INITIALIZER };
	if (src)
	{
		dst.versions = updateVersionList_clone(&src->versions);
		dst.stable = sb_clone(&src->stable);
		dst.latest = sb_clone(&src->latest);
	}
	return dst;
}

updateVersionName_t json_deserialize_updateVersionName_t(JSON_Value* src)
{
	updateVersionName_t dst;
	memset(&dst, 0, sizeof(dst));
	if (src)
	{
		JSON_Object* obj = json_value_get_object(src);
		if (obj)
		{
			dst.name = json_deserialize_sb_t(json_object_get_value(obj, "name"));
		}
	}
	return dst;
}

updateVersion_t json_deserialize_updateVersion_t(JSON_Value* src)
{
	updateVersion_t dst;
	memset(&dst, 0, sizeof(dst));
	if (src)
	{
		JSON_Object* obj = json_value_get_object(src);
		if (obj)
		{
			dst.name = json_deserialize_sb_t(json_object_get_value(obj, "name"));
			dst.path = json_deserialize_sb_t(json_object_get_value(obj, "path"));
		}
	}
	return dst;
}

updateVersionList_t json_deserialize_updateVersionList_t(JSON_Value* src)
{
	updateVersionList_t dst;
	memset(&dst, 0, sizeof(dst));
	if (src)
	{
		JSON_Array* arr = json_value_get_array(src);
		if (arr)
		{
			for (u32 i = 0; i < json_array_get_count(arr); ++i)
			{
				bba_push(dst, json_deserialize_updateVersion_t(json_array_get_value(arr, i)));
			}
		}
	}
	return dst;
}

updateManifest_t json_deserialize_updateManifest_t(JSON_Value* src)
{
	updateManifest_t dst;
	memset(&dst, 0, sizeof(dst));
	if (src)
	{
		JSON_Object* obj = json_value_get_object(src);
		if (obj)
		{
			dst.versions = json_deserialize_updateVersionList_t(json_object_get_value(obj, "versions"));
			dst.stable = json_deserialize_sb_t(json_object_get_value(obj, "stable"));
			dst.latest = json_deserialize_sb_t(json_object_get_value(obj, "latest"));
		}
	}
	return dst;
}

JSON_Value* json_serialize_updateVersionName_t(const updateVersionName_t* src)
{
	JSON_Value* val = json_value_init_object();
	JSON_Object* obj = json_value_get_object(val);
	if (obj)
	{
		json_object_set_value(obj, "name", json_serialize_sb_t(&src->name));
	}
	return val;
}

JSON_Value* json_serialize_updateVersion_t(const updateVersion_t* src)
{
	JSON_Value* val = json_value_init_object();
	JSON_Object* obj = json_value_get_object(val);
	if (obj)
	{
		json_object_set_value(obj, "name", json_serialize_sb_t(&src->name));
		json_object_set_value(obj, "path", json_serialize_sb_t(&src->path));
	}
	return val;
}

JSON_Value* json_serialize_updateVersionList_t(const updateVersionList_t* src)
{
	JSON_Value* val = json_value_init_array();
	JSON_Array* arr = json_value_get_array(val);
	if (arr)
	{
		for (u32 i = 0; i < src->count; ++i)
		{
			JSON_Value* child = json_serialize_updateVersion_t(src->data + i);
			if (child)
			{
				json_array_append_value(arr, child);
			}
		}
	}
	return val;
}

JSON_Value* json_serialize_updateManifest_t(const updateManifest_t* src)
{
	JSON_Value* val = json_value_init_object();
	JSON_Object* obj = json_value_get_object(val);
	if (obj)
	{
		json_object_set_value(obj, "versions", json_serialize_updateVersionList_t(&src->versions));
		json_object_set_value(obj, "stable", json_serialize_sb_t(&src->stable));
		json_object_set_value(obj, "latest", json_serialize_sb_t(&src->latest));
	}
	return val;
}
