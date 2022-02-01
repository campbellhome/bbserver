#define _HAS_EXCEPTIONS 0
#define _ITERATOR_DEBUG_LEVEL 0

#include "mc_preproc.h"

static void GenerateJsonHeader(const char *prefix, const char *srcDir)
{
	std::string s;

	InitialComments(s);
	va(s, "\n");
	va(s, "#pragma once\n");
	va(s, "\n");
	va(s, "#include \"bb.h\" // pull in stdint.h warning suppression for VS 2019 16.4 when compiling .c files\n");
	va(s, "#include \"parson/parson.h\"\n");
	va(s, "\n");
	va(s, "#if defined(__cplusplus)\n");
	va(s, "extern \"C\" {\n");
	va(s, "#endif\n");
	va(s, "\n");
	ForwardDeclarations(s);
	va(s, "\n");
	for(const enum_s &o : g_enums) {
		if(o.typedefBaseName.empty()) {
			va(s, "enum %s;\n", o.name.c_str());
		} else {
			va(s, "typedef enum %s %s;\n", o.typedefBaseName.c_str(), o.name.c_str());
		}
	}
	va(s, "\n");
	for(const struct_s &o : g_structs) {
		if(!o.jsonSerialization)
			continue;
		va(s, "%s json_deserialize_%s(JSON_Value *src);\n", o.name.c_str(), o.name.c_str());
	}
	va(s, "\n");
	for(const struct_s &o : g_structs) {
		if(!o.jsonSerialization)
			continue;
		va(s, "JSON_Value *json_serialize_%s(const %s *src);\n", o.name.c_str(), o.name.c_str());
	}
	va(s, "\n");
	va(s, "\n");
	for(const enum_s &o : g_enums) {
		va(s, "%s json_deserialize_%s(JSON_Value *src);\n", o.name.c_str(), o.name.c_str());
	}
	va(s, "\n");
	for(const enum_s &o : g_enums) {
		va(s, "JSON_Value *json_serialize_%s(const %s src);\n", o.name.c_str(), o.name.c_str());
	}
	va(s, "\n");
	va(s, "\n");
	for(const enum_s &o : g_enums) {
		va(s, "%s %s_from_string(const char *src);\n", o.name.c_str(), o.name.c_str());
	}
	va(s, "\n");
	for(const enum_s &o : g_enums) {
		va(s, "const char *string_from_%s(const %s src);\n", o.name.c_str(), o.name.c_str());
	}
	va(s, "\n");
	va(s, "#if defined(__cplusplus)\n");
	va(s, "} // extern \"C\"\n");
	va(s, "#endif\n");

	s = ReplaceChar(s, '\n', "\r\n");
	WriteAndReportFileData(s, srcDir, prefix, "json_generated.h");
}

enum memberJsonType_e {
	kMemberJsonObject,
	kMemberJsonBoolean,
	kMemberJsonNumber,
	kMemberJsonEnum,
};

static memberJsonType_e ClassifyMemberJson(const struct_member_s &m)
{
	if(m.typeStr == "bool" || m.typeStr == "b32" || m.typeStr == "b8") {
		return kMemberJsonBoolean;
	}
	for(const struct_s &s : g_structs) {
		if(s.name == m.typeStr) {
			return kMemberJsonObject;
		}
	}
	for(const enum_s &e : g_enums) {
		if(e.name == m.typeStr) {
			return kMemberJsonEnum;
		}
	}
	return kMemberJsonNumber;
}

static void GenerateJsonSource(const char *prefix, const char *includePrefix, const char *srcDir)
{
	std::string s;

	InitialComments(s);
	va(s, "\n");
	va(s, "#include \"%s%sjson_generated.h\"\n", includePrefix, prefix);
	va(s, "#include \"bb_array.h\"\n");
	va(s, "#include \"json_utils.h\"\n");
	va(s, "#include \"va.h\"\n");
	va(s, "\n");
	for(const std::string &str : g_paths) {
		std::string path = ReplaceChar(str, '\\', "/");
		va(s, "#include \"%s\"\n", path.c_str());
	}
	va(s, "\n");
	va(s, "//////////////////////////////////////////////////////////////////////////\n");
	va(s, "\n");
	for(const struct_s &o : g_structs) {
		if(o.headerOnly || !o.jsonSerialization)
			continue;
		const struct_member_s *m_count = nullptr;
		const struct_member_s *m_allocated = nullptr;
		const struct_member_s *m_data = nullptr;
		for(const struct_member_s &m : o.members) {
			if(m.name == "count") {
				m_count = &m;
			} else if(m.name == "allocated") {
				m_allocated = &m;
			} else if(m.name == "data") {
				m_data = &m;
			}
		}
		if(o.members.size() == 3 && m_count && m_allocated && m_data) {
			va(s, "%s json_deserialize_%s(JSON_Value *src)\n", o.name.c_str(), o.name.c_str());
			va(s, "{\n");
			va(s, "\t%s dst;\n", o.name.c_str());
			va(s, "\tmemset(&dst, 0, sizeof(dst));\n");
			va(s, "\tif(src) {\n");
			va(s, "\t\tJSON_Array *arr = json_value_get_array(src);\n");
			va(s, "\t\tif(arr) {\n");
			va(s, "\t\t\tfor(u32 i = 0; i < json_array_get_count(arr); ++i) {\n");
			va(s, "\t\t\t\tbba_push(dst, json_deserialize_%.*s(json_array_get_value(arr, i)));\n", m_data->typeStr.length() - 1, m_data->typeStr.c_str());
			va(s, "\t\t\t}\n");
			va(s, "\t\t}\n");
			va(s, "\t}\n");
			va(s, "\treturn dst;\n");
			va(s, "}\n");
			va(s, "\n");
			continue;
		}
		va(s, "%s json_deserialize_%s(JSON_Value *src)\n", o.name.c_str(), o.name.c_str());
		va(s, "{\n");
		va(s, "\t%s dst;\n", o.name.c_str());
		va(s, "\tmemset(&dst, 0, sizeof(dst));\n");
		va(s, "\tif(src) {\n");
		va(s, "\t\tJSON_Object *obj = json_value_get_object(src);\n");
		va(s, "\t\tif(obj) {\n");
		for(const struct_member_s &m : o.members) {
			if(m.arr.empty()) {
				switch(ClassifyMemberJson(m)) {
				case kMemberJsonObject:
					va(s, "\t\t\tdst.%s = json_deserialize_%s(json_object_get_value(obj, \"%s\"));\n",
					   m.name.c_str(), m.typeStr.c_str(), m.name.c_str());
					break;
				case kMemberJsonEnum:
					va(s, "\t\t\tdst.%s = json_deserialize_%s(json_object_get_value(obj, \"%s\"));\n",
					   m.name.c_str(), m.typeStr.c_str(), m.name.c_str());
					break;
				case kMemberJsonNumber:
					va(s, "\t\t\tdst.%s = (%s)json_object_get_number(obj, \"%s\");\n",
					   m.name.c_str(), m.typeStr.c_str(), m.name.c_str());
					break;
				case kMemberJsonBoolean:
					va(s, "\t\t\tdst.%s = json_object_get_boolean_safe(obj, \"%s\");\n",
					   m.name.c_str(), m.name.c_str());
					break;
				}
			} else {
				va(s, "\t\t\tfor(u32 i = 0; i < BB_ARRAYSIZE(dst.%s); ++i) {\n", m.name.c_str());
				switch(ClassifyMemberJson(m)) {
				case kMemberJsonObject:
					va(s, "\t\t\t\tdst.%s[i] = json_deserialize_%s(json_object_get_value(obj, va(\"%s.%%u\", i)));\n",
					   m.name.c_str(), m.typeStr.c_str(), m.name.c_str());
					break;
				case kMemberJsonEnum:
					va(s, "\t\t\t\tdst.%s[i] = json_deserialize_%s(json_object_get_value(obj, va(\"%s.%%u\", i)));\n",
					   m.name.c_str(), m.typeStr.c_str(), m.name.c_str());
					break;
				case kMemberJsonNumber:
					va(s, "\t\t\t\tdst.%s[i] = (%s)json_object_get_number(obj, va(\"%s.%%u\", i));\n",
					   m.name.c_str(), m.typeStr.c_str(), m.name.c_str());
					break;
				case kMemberJsonBoolean:
					va(s, "\t\t\t\tdst.%s[i] = json_object_get_boolean_safe(obj, va(\"%s.%%u\", i));\n",
					   m.name.c_str(), m.name.c_str());
					break;
				}
				va(s, "\t\t\t}\n");
			}
		}
		va(s, "\t\t}\n");
		va(s, "\t}\n");
		va(s, "\treturn dst;\n");
		va(s, "}\n");
		va(s, "\n");
	}
	va(s, "//////////////////////////////////////////////////////////////////////////\n");
	va(s, "\n");
	for(const struct_s &o : g_structs) {
		if(o.headerOnly || !o.jsonSerialization)
			continue;
		const struct_member_s *m_count = nullptr;
		const struct_member_s *m_allocated = nullptr;
		const struct_member_s *m_data = nullptr;
		for(const struct_member_s &m : o.members) {
			if(m.name == "count") {
				m_count = &m;
			} else if(m.name == "allocated") {
				m_allocated = &m;
			} else if(m.name == "data") {
				m_data = &m;
			}
		}
		if(o.members.size() == 3 && m_count && m_allocated && m_data) {
			va(s, "JSON_Value *json_serialize_%s(const %s *src)\n", o.name.c_str(), o.name.c_str());
			va(s, "{\n");
			va(s, "\tJSON_Value *val = json_value_init_array();\n");
			va(s, "\tJSON_Array *arr = json_value_get_array(val);\n");
			va(s, "\tif(arr) {\n");
			va(s, "\t\tfor(u32 i = 0; i < src->count; ++i) {\n");
			va(s, "\t\t\tJSON_Value *child = json_serialize_%.*s(src->data + i);\n", m_data->typeStr.length() - 1, m_data->typeStr.c_str());
			va(s, "\t\t\tif(child) {\n");
			va(s, "\t\t\t\tjson_array_append_value(arr, child);\n");
			va(s, "\t\t\t}\n");
			va(s, "\t\t}\n");
			va(s, "\t}\n");
			va(s, "\treturn val;\n");
			va(s, "}\n");
			va(s, "\n");
			continue;
		}
		va(s, "JSON_Value *json_serialize_%s(const %s *src)\n", o.name.c_str(), o.name.c_str());
		va(s, "{\n");
		va(s, "\tJSON_Value *val = json_value_init_object();\n");
		va(s, "\tJSON_Object *obj = json_value_get_object(val);\n");
		va(s, "\tif(obj) {\n");
		for(const struct_member_s &m : o.members) {
			if(m.arr.empty()) {
				switch(ClassifyMemberJson(m)) {
				case kMemberJsonObject:
					va(s, "\t\tjson_object_set_value(obj, \"%s\", json_serialize_%s(&src->%s));\n",
					   m.name.c_str(), m.typeStr.c_str(), m.name.c_str());
					break;
				case kMemberJsonEnum:
					va(s, "\t\tjson_object_set_value(obj, \"%s\", json_serialize_%s(src->%s));\n",
					   m.name.c_str(), m.typeStr.c_str(), m.name.c_str());
					break;
				case kMemberJsonNumber:
					va(s, "\t\tjson_object_set_number(obj, \"%s\", src->%s);\n",
					   m.name.c_str(), m.name.c_str());
					break;
				case kMemberJsonBoolean:
					va(s, "\t\tjson_object_set_boolean(obj, \"%s\", src->%s);\n",
					   m.name.c_str(), m.name.c_str());
					break;
				}
			} else {
				va(s, "\t\tfor(u32 i = 0; i < BB_ARRAYSIZE(src->%s); ++i) {\n", m.name.c_str());
				switch(ClassifyMemberJson(m)) {
				case kMemberJsonObject:
					va(s, "\t\t\tjson_object_set_value(obj, va(\"%s.%%u\", i), json_serialize_%s(&src->%s[i]));\n",
					   m.name.c_str(), m.typeStr.c_str(), m.name.c_str());
					break;
				case kMemberJsonEnum:
					va(s, "\t\t\tjson_object_set_value(obj, va(\"%s.%%u\", i), json_serialize_%s(src->%s[i]));\n",
					   m.name.c_str(), m.typeStr.c_str(), m.name.c_str());
					break;
				case kMemberJsonNumber:
					va(s, "\t\t\tjson_object_set_number(obj, va(\"%s.%%u\", i), src->%s[i]);\n",
					   m.name.c_str(), m.name.c_str());
					break;
				case kMemberJsonBoolean:
					va(s, "\t\t\tjson_object_set_boolean(obj, va(\"%s.%%u\", i), src->%s[i]);\n",
					   m.name.c_str(), m.name.c_str());
					break;
				}
				va(s, "\t\t}\n");
			}
		}
		va(s, "\t}\n");
		va(s, "\treturn val;\n");
		va(s, "}\n");
		va(s, "\n");
	}
	va(s, "//////////////////////////////////////////////////////////////////////////\n");
	va(s, "\n");
	for(const enum_s &o : g_enums) {
		if(o.headerOnly)
			continue;
		va(s, "%s json_deserialize_%s(JSON_Value *src)\n", o.name.c_str(), o.name.c_str());
		va(s, "{\n");
		va(s, "\t%s dst = %s;\n", o.name.c_str(), o.defaultVal.c_str());
		va(s, "\tif(src) {\n");
		va(s, "\t\tconst char *str = json_value_get_string(src);\n");
		va(s, "\t\tif(str) {\n");
		for(const enum_member_s &m : o.members) {
			va(s, "\t\t\tif(!strcmp(str, \"%s\")) { dst = %s; }\n", m.name.c_str(), m.name.c_str());
		}
		va(s, "\t\t}\n");
		va(s, "\t}\n");
		va(s, "\treturn dst;\n");
		va(s, "}\n");
		va(s, "\n");
	}
	va(s, "//////////////////////////////////////////////////////////////////////////\n");
	va(s, "\n");
	for(const enum_s &o : g_enums) {
		if(o.headerOnly)
			continue;
		va(s, "JSON_Value *json_serialize_%s(const %s src)\n", o.name.c_str(), o.name.c_str());
		va(s, "{\n");
		va(s, "\tconst char *str = \"\";\n");
		va(s, "\tswitch(src) {\n");
		for(const enum_member_s &m : o.members) {
			va(s, "\t\tcase %s: str = \"%s\"; break;\n", m.name.c_str(), m.name.c_str());
		}
		va(s, "\t}\n");
		va(s, "\tJSON_Value *val = json_value_init_string(str);\n");
		va(s, "\treturn val;\n");
		va(s, "}\n");
		va(s, "\n");
	}
	va(s, "//////////////////////////////////////////////////////////////////////////\n");
	va(s, "\n");
	for(const enum_s &o : g_enums) {
		if(o.headerOnly)
			continue;
		va(s, "%s %s_from_string(const char *src)\n", o.name.c_str(), o.name.c_str());
		va(s, "{\n");
		va(s, "\t%s dst = %s;\n", o.name.c_str(), o.defaultVal.c_str());
		va(s, "\tif(src) {\n");
		for(const enum_member_s &m : o.members) {
			va(s, "\t\tif(!strcmp(src, \"%s\")) { dst = %s; }\n", m.name.c_str(), m.name.c_str());
		}
		va(s, "\t}\n");
		va(s, "\treturn dst;\n");
		va(s, "}\n");
		va(s, "\n");
	}
	va(s, "//////////////////////////////////////////////////////////////////////////\n");
	va(s, "\n");
	for(const enum_s &o : g_enums) {
		if(o.headerOnly)
			continue;
		va(s, "const char *string_from_%s(const %s src)\n", o.name.c_str(), o.name.c_str());
		va(s, "{\n");
		va(s, "\tswitch(src) {\n");
		for(const enum_member_s &m : o.members) {
			va(s, "\t\tcase %s: return \"%s\"; break;\n", m.name.c_str(), m.name.c_str());
		}
		va(s, "\t}\n");
		va(s, "\treturn \"\";\n");
		va(s, "}\n");
		va(s, "\n");
	}
	va(s, "//////////////////////////////////////////////////////////////////////////\n");

	s = ReplaceChar(s, '\n', "\r\n");
	WriteAndReportFileData(s, srcDir, prefix, "json_generated.c");
}

void GenerateJson(const char *prefix, const char *includePrefix, const char *srcDir, const char *includeDir)
{
	GenerateJsonHeader(prefix, includeDir);
	GenerateJsonSource(prefix, includePrefix, srcDir);
}
