// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "mc_preproc_config.h"
#include "../../../thirdparty/parson/parson.h"
#include "bb_array.h"

static int json_object_get_boolean_safe(const JSON_Object* object, const char* name)
{
	return json_value_get_boolean(json_object_get_value(object, name)) == 1;
}

static std::string json_deserialize_std_string(JSON_Value* src)
{
	std::string dst;
	const char* str = json_value_get_string(src);
	if (str)
	{
		dst = str;
	}
	return dst;
}

preprocInputDir json_deserialize_preprocInputDir(JSON_Value* src)
{
	preprocInputDir dst;
	JSON_Object* obj = src ? json_value_get_object(src) : nullptr;
	if (obj)
	{
		dst.dir = json_deserialize_std_string(json_object_get_value(obj, "dir"));
		dst.base = json_deserialize_std_string(json_object_get_value(obj, "base"));
	}
	return dst;
}

std::vector<preprocInputDir> json_deserialize_preprocInputDirs(JSON_Value* src)
{
	std::vector<preprocInputDir> dst;
	JSON_Array* arr = src ? json_value_get_array(src) : nullptr;
	if (arr)
	{
		for (u32 i = 0; i < json_array_get_count(arr); ++i)
		{
			dst.push_back(json_deserialize_preprocInputDir(json_array_get_value(arr, i)));
		}
	}
	return dst;
}

preprocInputConfig json_deserialize_preprocInputConfig(JSON_Value* src)
{
	preprocInputConfig dst;
	JSON_Object* obj = src ? json_value_get_object(src) : nullptr;
	if (obj)
	{
		dst.checkFonts = json_object_get_boolean_safe(obj, "checkFonts");
		dst.sourceDirs = json_deserialize_preprocInputDirs(json_object_get_value(obj, "sourceDirs"));
		dst.includeDirs = json_deserialize_preprocInputDirs(json_object_get_value(obj, "includeDirs"));
	}
	return dst;
}

preprocOutputConfig json_deserialize_preprocOutputConfig(JSON_Value* src)
{
	preprocOutputConfig dst;
	JSON_Object* obj = src ? json_value_get_object(src) : nullptr;
	if (obj)
	{
		dst.prefix = json_deserialize_std_string(json_object_get_value(obj, "prefix"));
		dst.sourceDir = json_deserialize_std_string(json_object_get_value(obj, "sourceDir"));
		dst.includeDir = json_deserialize_std_string(json_object_get_value(obj, "includeDir"));
		dst.baseDir = json_deserialize_std_string(json_object_get_value(obj, "baseDir"));
	}
	return dst;
}

preprocConfig json_deserialize_preprocConfig(JSON_Value* src)
{
	preprocConfig dst;
	JSON_Object* obj = src ? json_value_get_object(src) : nullptr;
	if (obj)
	{
		dst.bb = json_object_get_boolean_safe(obj, "bb");
		dst.input = json_deserialize_preprocInputConfig(json_object_get_value(obj, "input"));
		dst.output = json_deserialize_preprocOutputConfig(json_object_get_value(obj, "output"));
	}
	return dst;
}

preprocConfig read_preprocConfig(const char* path)
{
	preprocConfig dst;
	JSON_Value* val = json_parse_file(path);
	if (val)
	{
		dst = json_deserialize_preprocConfig(val);
		json_value_free(val);
	}
	return dst;
}
