#define _HAS_EXCEPTIONS 0
#define _ITERATOR_DEBUG_LEVEL 0

// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#define LEXER_STATIC
#define wby_size lexer_size
#include "../../../thirdparty/mmx/lexer.h"

#include <set>
#include <stdarg.h>
#include <string>
#include <vector>

struct enum_member_s
{
	std::string name;
};
struct enum_s
{
	std::string name;
	std::string typedefBaseName;
	std::string defaultVal;
	std::vector<enum_member_s> members;
	bool headerOnly;
};
extern std::vector<enum_s> g_enums;

struct struct_member_s
{
	std::string name;
	std::string val;
	std::string arr;
	std::string typeStr;
	std::vector<lexer_token> typeTokens;
	bool parseempty = false;
};
struct struct_s
{
	std::string name;
	std::string typedefBaseName;
	bool autovalidate;
	bool headerOnly;
	bool fromLoc;
	bool jsonSerialization;
	bool bStringHash;
	std::vector<struct_member_s> members;
};
extern std::vector<struct_s> g_structs;

extern std::set<std::string> g_paths;

void GenerateJson(const char* prefix, const char* includePrefix, const char* srcDir, const char* includeDir);
void GenerateStructs(const char* prefix, const char* includePrefix, const char* srcDir, const char* includeDir);

std::string& va(std::string& dst, const char* fmt, ...);
const char* va(const char* fmt, ...);

std::string ReplaceChar(const std::string& src, char test, const char* replacement);

const char* GetPathFilename(const std::string& src);
const char* GetPathFilename(const char* src);
std::vector<std::string> TokenizePath(const std::string& src);
void AddPathComponent(std::string& path, const std::string& component, char separator = 0);
std::string ResolvePath(const std::string& src, char separator = 0);
void TestResolvePath();

std::string ReadFileContents(const std::string& path);
void WriteAndReportFileData(const std::string& data, const char* srcDir, const char* prefix, const char* suffix);

void ForwardDeclarations(std::string& s);
void InitialComments(std::string& s);
