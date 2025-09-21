#define _HAS_EXCEPTIONS 0
#define _ITERATOR_DEBUG_LEVEL 0

#include "mc_preproc.h"

#include "bb_common.h"

static bool EndsWith(const std::string& str, const char* substr)
{
	bool ret = false;
	size_t substrLen = strlen(substr);
	size_t strLen = str.length();
	if (strLen > substrLen)
	{
		const char* end = str.c_str() + (strLen - substrLen);
		if (!strcmp(end, substr))
		{
			ret = true;
		}
	}
	return ret;
}

static const char* GetMemberType(const std::string& typeStr)
{
	size_t len = typeStr.length();
	if (typeStr == "sb_t")
	{
		return "sb";
	}
	else if (typeStr == "sdict_t")
	{
		return "sdict";
	}
	else if (EndsWith(typeStr, "_t*"))
	{
		return va("%.*s", typeStr.length() - 3, typeStr.c_str());
	}
	else if (EndsWith(typeStr, "_t"))
	{
		return va("%.*s", typeStr.length() - 2, typeStr.c_str());
	}
	else if (EndsWith(typeStr, "*"))
	{
		return va("%.*s", typeStr.length() - 1, typeStr.c_str());
	}
	else
	{
		return typeStr.c_str();
	}
}

const struct_s* GetStruct(const std::string& name)
{
	std::string trimmed = name;
	if (trimmed.back() == '*')
	{
		trimmed.pop_back();
	}
	for (const struct_s& o : g_structs)
	{
		if (o.name != trimmed)
			continue;
		return &o;
	}
	return nullptr;
}

static bool IsFromLoc(const std::string& name)
{
	const struct_s* s = GetStruct(name);
	return s && s->fromLoc;
}

struct BBArrayData
{
	const struct_member_s* count = nullptr;
	const struct_member_s* allocated = nullptr;
	const struct_member_s* data = nullptr;
	bool bValid = false;
	bool bExact = false;
};

BBArrayData ParseBBArray(const struct_s& o)
{
	BBArrayData data = { BB_EMPTY_INITIALIZER };
	for (const struct_member_s& m : o.members)
	{
		if (m.name == "count")
		{
			data.count = &m;
		}
		else if (m.name == "allocated")
		{
			data.allocated = &m;
		}
		else if (m.name == "data")
		{
			data.data = &m;
		}
	}
	data.bValid = data.count && data.allocated && data.data;
	data.bExact = o.members.size() == 3 && data.bValid;
	return data;
}

struct HashTableData
{
	BBArrayData tableArray;
	BBArrayData chainArray;

	const struct_s* tableStruct = nullptr;
	const struct_s* chainStruct = nullptr;
	const struct_s* entryStruct = nullptr;
};

HashTableData GetHashTableData(const struct_s& o)
{
	HashTableData result;
	result.tableArray = ParseBBArray(o);
	result.tableStruct = &o;
	if (result.tableArray.bValid)
	{
		result.chainStruct = GetStruct(result.tableArray.data->typeStr);
		if (result.chainStruct)
		{
			result.chainArray = ParseBBArray(*result.chainStruct);
			if (result.chainArray.bValid)
			{
				result.entryStruct = GetStruct(result.chainArray.data->typeStr);
			}
		}
	}
	return result;
}

const struct_s* GetHashEntryStruct(const struct_s& o)
{
	if (!o.bStringHash)
		return nullptr;

	BBArrayData tableArray = ParseBBArray(o);
	if (!tableArray.bValid)
		return nullptr;

	const struct_s* chainStruct = GetStruct(tableArray.data->typeStr);
	if (!chainStruct)
		return nullptr;

	BBArrayData chainArray = ParseBBArray(*chainStruct);
	if (!chainArray.bValid)
		return nullptr;

	const struct_s* entryStruct = GetStruct(chainArray.data->typeStr);
	if (!entryStruct)
		return nullptr;

	return entryStruct;
}

void GenerateStringHashHeader(std::string& s)
{
	for (const struct_s& o : g_structs)
	{
		if (!o.bStringHash)
			continue;

		HashTableData ht = GetHashTableData(o);
		const struct_s* entry = ht.entryStruct;
		if (!entry)
			continue;

		if (o.fromLoc)
		{
			va(s, "\n");
			const char* entryShortName = GetMemberType(entry->name);
			const char* entryFullName = entry->name.c_str();
			const char* tableShortName = GetMemberType(o.name);
			const char* tableFullName = o.name.c_str();
			va(s, "u64 %s_hash(const %s *entry);\n", entryShortName, entryFullName);
			va(s, "int %s_compare(const %s *a, const %s *b);\n", entryShortName, entryFullName, entryFullName);
			va(s, "%s *%s_find(%s *table, const char *name);\n", entryFullName, tableShortName, tableFullName);
			va(s, "%s *%s_insert(%s *table, const %s *entry);\n", entryFullName, tableShortName, tableFullName, entryFullName);
			va(s, "%s *%s_insertmulti(%s *table, const %s *entry);\n", entryFullName, tableShortName, tableFullName, entryFullName);
			va(s, "%s *%s_find_internal(%s *table, const %s *entry, u64 hashValue);\n", entryFullName, tableShortName, tableFullName, entryFullName);
			va(s, "%s *%s_insert_internal(%s *table, const %s *entry, u64 hashValue);\n", entryFullName, tableShortName, tableFullName, entryFullName);
			va(s, "%s *%s_insertmulti_internal(%s *table, const %s *entry, u64 hashValue);\n", entryFullName, tableShortName, tableFullName, entryFullName);
			va(s, "void %s_remove(%s *table, const char *name);\n", tableShortName, tableFullName);
			va(s, "void %s_removemulti(%s *table, const char *name);\n", tableShortName, tableFullName);
		}
	}
}

void GenerateStringHashSource(std::string& s)
{
	for (const struct_s& o : g_structs)
	{
		if (!o.bStringHash)
			continue;

		HashTableData ht = GetHashTableData(o);
		if (!ht.entryStruct)
			continue;

		if (o.fromLoc)
		{
			const char* tableShortName = GetMemberType(ht.tableStruct->name);
			const char* tableFullName = ht.tableStruct->name.c_str();
			const char* chainShortName = GetMemberType(ht.chainStruct->name);
			const char* chainFullName = ht.chainStruct->name.c_str();
			const char* entryShortName = GetMemberType(ht.entryStruct->name);
			const char* entryFullName = ht.entryStruct->name.c_str();

			va(s, "\n");
			va(s, "u64 %s_hash(const %s *entry)\n", entryShortName, entryFullName);
			va(s, "{\n");
			va(s, "	return strsimplehash(sb_get(&entry->key));\n");
			va(s, "}\n");

			va(s, "\n");
			va(s, "int %s_compare(const %s *a, const %s *b)\n", entryShortName, entryFullName, entryFullName);
			va(s, "{\n");
			va(s, "	return strcmp(sb_get(&a->key), sb_get(&b->key));\n");
			va(s, "}\n");

			va(s, "\n");
			va(s, "%s *%s_find_internal(%s *table, const %s *entry, u64 hashValue)\n", entryFullName, tableShortName, tableFullName, entryFullName);
			va(s, "{\n");
			va(s, "	BB_ASSERT_MSG(table->count, \"need to initialize hash table count\");\n");
			va(s, "	u32 chainIndex = hashValue %% table->count;\n");
			va(s, "	%s *chain = table->data + chainIndex;\n", chainFullName);
			va(s, "	for(u32 i = 0; i < chain->count; ++i) {\n");
			va(s, "		%s *existing = chain->data + i;\n", entryFullName);
			va(s, "		if(!%s_compare(existing, entry)) {\n", entryShortName);
			va(s, "			return existing;\n");
			va(s, "		}\n");
			va(s, "	}\n");
			va(s, "	return NULL;\n");
			va(s, "}\n");

			va(s, "\n");
			va(s, "%s *%s_insert_internal(%s *table, const %s *entry, u64 hashValue)\n", entryFullName, tableShortName, tableFullName, entryFullName);
			va(s, "{\n");
			va(s, "	BB_ASSERT_MSG(table->count, \"need to initialize hash table count\");\n");
			va(s, "	%s *result = %s_find_internal(table, entry, hashValue);\n", entryFullName, tableFullName);
			va(s, "	if(!result) {\n");
			va(s, "		u32 chainIndex = hashValue %% table->count;\n");
			va(s, "		%s *chain = table->data + chainIndex;\n", chainFullName);
			va(s, "		result = bba_add_noclear(*chain, 1);\n");
			va(s, "		if(result != NULL) {\n");
			va(s, "			*result = %s_clone(entry);\n", entryShortName);
			va(s, "		}\n");
			va(s, "	}\n");
			va(s, "	\n");
			va(s, "	return result;\n");
			va(s, "}\n");

			va(s, "\n");
			va(s, "%s *%s_insertmulti_internal(%s *table, const %s *entry, u64 hashValue)\n", entryFullName, tableShortName, tableFullName, entryFullName);
			va(s, "{\n");
			va(s, "	u32 chainIndex = hashValue %% table->count;\n");
			va(s, "	%s *chain = table->data + chainIndex;\n", chainFullName);
			va(s, "	%s *result = bba_add_noclear(*chain, 1);\n", entryFullName);
			va(s, "	if(result != NULL) {\n");
			va(s, "		*result = %s_clone(entry);\n", entryShortName);
			va(s, "	}\n");
			va(s, "	\n");
			va(s, "	return result;\n");
			va(s, "}\n");

			va(s, "\n");
			va(s, "%s *%s_find(%s *table, const char *name)\n", entryFullName, tableShortName, tableFullName);
			va(s, "{\n");
			va(s, "	if(!name) {\n");
			va(s, "		name = \"\";\n");
			va(s, "	}\n");
			va(s, "	%s entry = { BB_EMPTY_INITIALIZER };\n", entryFullName);
			va(s, "	entry.key = sb_from_c_string_no_alloc(name);\n");
			va(s, "	u64 hashValue = %s_hash(&entry);\n", entryShortName);
			va(s, "	return %s_find_internal(table, &entry, hashValue);\n", tableShortName);
			va(s, "}\n");

			va(s, "\n");
			va(s, "%s *%s_insert(%s *table, const %s *entry)\n", entryFullName, tableShortName, tableFullName, entryFullName);
			va(s, "{\n");
			va(s, "	u64 hashValue = %s_hash(entry);\n", entryShortName);
			va(s, "	return %s_insert_internal(table, entry, hashValue);\n", tableShortName);
			va(s, "}\n");

			va(s, "\n");
			va(s, "%s *%s_insertmulti(%s *table, const %s *entry)\n", entryFullName, tableShortName, tableFullName, entryFullName);
			va(s, "{\n");
			va(s, "	u64 hashValue = %s_hash(entry);\n", entryShortName);
			va(s, "	return %s_insertmulti_internal(table, entry, hashValue);\n", tableShortName);
			va(s, "}\n");

			va(s, "\n");
			va(s, "void %s_remove(%s *table, const char *name)\n", tableShortName, tableFullName);
			va(s, "{\n");
			va(s, "	%s entry = { BB_EMPTY_INITIALIZER };\n", entryFullName);
			va(s, "	entry.key = sb_from_c_string_no_alloc(name);\n");
			va(s, "	u64 hashValue = %s_hash(&entry);\n", entryShortName);
			va(s, "\n");
			va(s, "	u32 chainIndex = hashValue %% table->count;\n");
			va(s, "	%s *chain = table->data + chainIndex;\n", chainFullName);
			va(s, "	for(u32 i = 0; i < chain->count; ++i) {\n");
			va(s, "		%s *existing = chain->data + i;\n", entryFullName);
			va(s, "		if(!%s_compare(existing, &entry)) {\n", entryShortName);
			va(s, "			%s_reset(existing);\n", entryShortName);
			va(s, "			bba_erase(*chain, i);\n");
			va(s, "			return;\n");
			va(s, "		}\n");
			va(s, "	}\n");
			va(s, "}\n");

			va(s, "\n");
			va(s, "void %s_removemulti(%s *table, const char *name)\n", tableShortName, tableFullName);
			va(s, "{\n");
			va(s, "	%s entry = { BB_EMPTY_INITIALIZER };\n", entryFullName);
			va(s, "	entry.key = sb_from_c_string_no_alloc(name);\n");
			va(s, "	u64 hashValue = %s_hash(&entry);\n", entryShortName);
			va(s, "\n");
			va(s, "	u32 chainIndex = hashValue %% table->count;\n");
			va(s, "	%s *chain = table->data + chainIndex;\n", chainFullName);
			va(s, "	for(u32 i = 0; i < chain->count;) {\n");
			va(s, "		%s *existing = chain->data + i;\n", entryFullName);
			va(s, "		if(%s_compare(existing, &entry)) {\n", entryShortName);
			va(s, "			++i;\n");
			va(s, "		} else {\n");
			va(s, "			%s_reset(existing);\n", entryShortName);
			va(s, "			bba_erase(*chain, i);\n");
			va(s, "		}\n");
			va(s, "	}\n");
			va(s, "}\n");
		}
	}
}

static void GenerateStructsHeader(const char* prefix, const char* srcDir)
{
	std::string s;

	InitialComments(s);
	va(s, "\n");
	va(s, "#pragma once\n");
	va(s, "\n");
	va(s, "#include \"bb_types.h\"\n");
	va(s, "\n");
	va(s, "#if defined(__cplusplus)\n");
	va(s, "extern \"C\" {\n");
	va(s, "#endif\n");
	va(s, "\n");
	ForwardDeclarations(s);
	va(s, "\n");
	for (const struct_s& o : g_structs)
	{
		if (o.fromLoc)
		{
			va(s, "void %s_reset_from_loc(const char *file, int line, %s *val);\n", GetMemberType(o.name), o.name.c_str());
		}
		else
		{
			va(s, "void %s_reset(%s *val);\n", GetMemberType(o.name), o.name.c_str());
		}
	}
	va(s, "\n");
	for (const struct_s& o : g_structs)
	{
		va(s, "#if !defined(__cplusplus) || defined(DECLARE_%s_clone)\n", GetMemberType(o.name), GetMemberType(o.name));
		if (o.fromLoc)
		{
			va(s, "%s %s_clone_from_loc(const char *file, int line, const %s *src);\n", o.name.c_str(), GetMemberType(o.name), o.name.c_str());
		}
		else
		{
			va(s, "%s %s_clone(const %s *src);\n", o.name.c_str(), GetMemberType(o.name), o.name.c_str());
		}
		va(s, "#endif\n");
	}
	va(s, "\n");
	va(s, "#if defined(__cplusplus)\n");
	va(s, "} // extern \"C\"\n");
	va(s, "#endif\n");

	va(s, "\n");
	for (const struct_s& o : g_structs)
	{
		if (o.fromLoc)
		{
			va(s, "#if !defined(%s_reset)\n", GetMemberType(o.name));
			va(s, "#define %s_reset(var) %s_reset_from_loc(__FILE__, __LINE__, var);\n", GetMemberType(o.name), GetMemberType(o.name));
			va(s, "#endif\n");
		}
	}

	va(s, "\n");
	for (const struct_s& o : g_structs)
	{
		if (o.fromLoc)
		{
			va(s, "#if !defined(%s_clone)\n", GetMemberType(o.name));
			va(s, "#define %s_clone(var) %s_clone_from_loc(__FILE__, __LINE__, var);\n", GetMemberType(o.name), GetMemberType(o.name));
			va(s, "#endif\n");
		}
	}

	GenerateStringHashHeader(s);

	s = ReplaceChar(s, '\n', "\r\n");
	WriteAndReportFileData(s, srcDir, prefix, "structs_generated.h");
}

enum memberJsonType_e
{
	kMemberJsonObject,
	kMemberJsonBoolean,
	kMemberJsonNumber,
	kMemberJsonEnum,
};

static memberJsonType_e ClassifyMemberJson(const struct_member_s& m)
{
	if (m.typeStr == "bool" || m.typeStr == "b32" || m.typeStr == "b8")
	{
		return kMemberJsonBoolean;
	}
	for (const struct_s& s : g_structs)
	{
		if (s.name == m.typeStr)
		{
			return kMemberJsonObject;
		}
	}
	for (const enum_s& e : g_enums)
	{
		if (e.name == m.typeStr)
		{
			return kMemberJsonEnum;
		}
	}
	return kMemberJsonNumber;
}

static void GenerateStructsSource(const char* prefix, const char* includePrefix, const char* srcDir)
{
	std::string s;

	InitialComments(s);
	va(s, "\n");
	va(s, "#include \"%s%sstructs_generated.h\"\n", includePrefix, prefix);
	va(s, "#include \"bb_assert.h\"\n");
	va(s, "#include \"bb_array.h\"\n");
	va(s, "#include \"str.h\"\n");
	va(s, "#include \"va.h\"\n");
	va(s, "\n");
	for (const std::string& str : g_paths)
	{
		std::string path = ReplaceChar(str, '\\', "/");
		va(s, "#include \"%s\"\n", path.c_str());
	}
	va(s, "\n");
	va(s, "#include <string.h>\n");
	va(s, "\n");
	for (const struct_s& o : g_structs)
	{
		if (o.headerOnly)
			continue;
		BBArrayData bbArrayData = ParseBBArray(o);
		va(s, "\n");
		if (o.fromLoc)
		{
			va(s, "void %s_reset_from_loc(const char *file, int line, %s *val)\n", GetMemberType(o.name), o.name.c_str());
		}
		else
		{
			va(s, "void %s_reset(%s *val)\n", GetMemberType(o.name), o.name.c_str());
		}
		va(s, "{\n");
		va(s, "\tif(val) {\n");
		if (bbArrayData.bValid)
		{
			va(s, "\t\tfor(u32 i = 0; i < val->count; ++i) {\n");
			if (o.fromLoc && IsFromLoc(bbArrayData.data->typeStr))
			{
				va(s, "\t\t\t%s_reset_from_loc(file, line, val->data + i);\n", GetMemberType(bbArrayData.data->typeStr));
			}
			else
			{
				va(s, "\t\t\t%s_reset(val->data + i);\n", GetMemberType(bbArrayData.data->typeStr));
			}
			va(s, "\t\t}\n");
			va(s, "\t\tbba_free(*val);\n", bbArrayData.data->name.c_str());
		}
		if (!bbArrayData.bExact)
		{
			for (const struct_member_s& m : o.members)
			{
				if (bbArrayData.bValid)
				{
					if (m.name == "data" || m.name == "count" || m.name == "allocated")
					{
						continue;
					}
				}
				if (ClassifyMemberJson(m) != kMemberJsonObject)
				{
					continue;
				}
				if (m.arr.empty())
				{
					if (o.fromLoc && IsFromLoc(m.typeStr))
					{
						va(s, "\t\t%s_reset_from_loc(file, line, &val->%s);\n", GetMemberType(m.typeStr), m.name.c_str());
					}
					else
					{
						va(s, "\t\t%s_reset(&val->%s);\n", GetMemberType(m.typeStr), m.name.c_str());
					}
				}
				else
				{
					va(s, "\t\tfor(u32 i = 0; i < BB_ARRAYSIZE(val->%s); ++i) {\n", m.name.c_str());
					if (o.fromLoc && IsFromLoc(m.typeStr))
					{
						va(s, "\t\t\t%s_reset_from_loc(file, line, val->%s + i);\n", GetMemberType(m.typeStr), m.name.c_str());
					}
					else
					{
						va(s, "\t\t\t%s_reset(val->%s + i);\n", GetMemberType(m.typeStr), m.name.c_str());
					}
					va(s, "\t\t}\n");
				}
			}
		}
		va(s, "\t}\n");
		va(s, "}\n");
		if (o.fromLoc)
		{
			va(s, "%s %s_clone_from_loc(const char *file, int line, const %s *src)\n", o.name.c_str(), GetMemberType(o.name), o.name.c_str());
		}
		else
		{
			va(s, "%s %s_clone(const %s *src)\n", o.name.c_str(), GetMemberType(o.name), o.name.c_str());
		}
		va(s, "{\n");
		va(s, "\t%s dst = { BB_EMPTY_INITIALIZER };\n", o.name.c_str());
		va(s, "\tif(src) {\n");
		if (bbArrayData.bValid)
		{
			va(s, "\t\tfor(u32 i = 0; i < src->count; ++i) {\n");
			va(s, "\t\t\tif(bba_add_noclear(dst, 1)) {\n");
			if (o.fromLoc && IsFromLoc(bbArrayData.data->typeStr))
			{
				va(s, "\t\t\t\tbba_last(dst) = %s_clone_from_loc(file, line, src->data + i);\n", GetMemberType(bbArrayData.data->typeStr));
			}
			else
			{
				va(s, "\t\t\t\tbba_last(dst) = %s_clone(src->data + i);\n", GetMemberType(bbArrayData.data->typeStr));
			}
			va(s, "\t\t\t}\n");
			va(s, "\t\t}\n");
		}
		if (!bbArrayData.bExact)
		{
			for (const struct_member_s& m : o.members)
			{
				if (bbArrayData.bValid)
				{
					if (m.name == "data" || m.name == "count" || m.name == "allocated")
					{
						continue;
					}
				}
				if (m.arr.empty())
				{
					if (ClassifyMemberJson(m) == kMemberJsonObject)
					{
						if (o.fromLoc && IsFromLoc(m.typeStr))
						{
							va(s, "\t\tdst.%s = %s_clone_from_loc(file, line, &src->%s);\n", m.name.c_str(), GetMemberType(m.typeStr), m.name.c_str());
						}
						else
						{
							va(s, "\t\tdst.%s = %s_clone(&src->%s);\n", m.name.c_str(), GetMemberType(m.typeStr), m.name.c_str());
						}
					}
					else
					{
						va(s, "\t\tdst.%s = src->%s;\n", m.name.c_str(), m.name.c_str());
					}
				}
				else
				{
					va(s, "\t\tfor(u32 i = 0; i < BB_ARRAYSIZE(src->%s); ++i) {\n", m.name.c_str());
					if (ClassifyMemberJson(m) == kMemberJsonObject)
					{
						if (o.fromLoc && IsFromLoc(m.typeStr))
						{
							va(s, "\t\t\tdst.%s[i] = %s_clone_from_loc(file, line, &src->%s[i]);\n", m.name.c_str(), GetMemberType(m.typeStr), m.name.c_str());
						}
						else
						{
							va(s, "\t\t\tdst.%s[i] = %s_clone(&src->%s[i]);\n", m.name.c_str(), GetMemberType(m.typeStr), m.name.c_str());
						}
					}
					else
					{
						va(s, "\t\t\tdst.%s[i] = src->%s[i];\n", m.name.c_str(), m.name.c_str());
					}
					va(s, "\t\t}\n");
				}
			}
		}
		va(s, "\t}\n");
		va(s, "\treturn dst;\n");
		va(s, "}\n");
	}

	GenerateStringHashSource(s);

	s = ReplaceChar(s, '\n', "\r\n");
	WriteAndReportFileData(s, srcDir, prefix, "structs_generated.c");
}

void GenerateStructs(const char* prefix, const char* includePrefix, const char* srcDir, const char* includeDir)
{
	GenerateStructsHeader(prefix, includeDir);
	GenerateStructsSource(prefix, includePrefix, srcDir);
}
