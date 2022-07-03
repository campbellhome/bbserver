#include "mc_preproc.h"

#include "bb_file.h"
#include "bb_string.h"
#include "mc_preproc_config.h"

#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEXER_IMPLEMENTATION
#define LEXER_STATIC
#include "../../../thirdparty/mmx/lexer.h"

#include "../../../thirdparty/parson/parson.c"

#include "bb_wrap_windows.h"

#if BB_USING(BB_PLATFORM_WINDOWS) && BB_USING(BB_COMPILER_MSVC) && defined(_DEBUG)
#include <crtdbg.h>
void crt_leak_check_init(void)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
}
#else
void crt_leak_check_init(void)
{
}
#endif

typedef struct span_s {
	const char *start;
	const char *end;
} span_t;

std::vector< enum_s > g_enums;
std::vector< struct_s > g_structs;
std::set< std::string > g_paths;
std::set< std::string > g_ignorePaths;
bool g_bHeaderOnly;
const char *g_includePrefix = "";

std::string lexer_token_string(const lexer_token &tok)
{
	char buffer[1024];
	buffer[0] = 0;
	if(tok.type == LEXER_TOKEN_STRING) {
		buffer[0] = '\"';
		lexer_size count = lexer_token_cpy(buffer + 1, 1022, &tok);
		buffer[count + 1] = '\"';
		buffer[count + 2] = 0;
	} else if(tok.type == LEXER_TOKEN_NUMBER && (tok.subtype & LEXER_TOKEN_SINGLE_PREC) != 0) {
		lexer_size count = lexer_token_cpy(buffer, 1023, &tok);
		buffer[count] = 'f';
		buffer[count + 1] = 0;
	} else {
		lexer_token_cpy(buffer, 1024, &tok);
	}
	return buffer;
}

bool mm_lexer_parse_enum(lexer *lex, std::string defaultVal, bool isTypedef, bool headerOnly)
{
	lexer_token name;
	if(!lexer_read_on_line(lex, &name))
		return false;

	BB_LOG("mm_lexer", "AUTOJSON enum %s", lexer_token_string(name).c_str());
	enum_s e = { lexer_token_string(name), "" };
	e.headerOnly = headerOnly;

	lexer_token tok;
	if(!lexer_expect_type(lex, LEXER_TOKEN_PUNCTUATION, LEXER_PUNCT_BRACE_OPEN, &tok))
		return false;

	while(1) {
		if(!lexer_read(lex, &tok))
			break;

		if(tok.type == LEXER_TOKEN_PUNCTUATION && tok.subtype == LEXER_PUNCT_BRACE_CLOSE) {
			BB_LOG("mm_lexer", "AUTOJSON end enum");
			if(isTypedef) {
				e.typedefBaseName = e.name;
				if(lexer_read(lex, &tok) && tok.type == LEXER_TOKEN_NAME) {
					e.name = lexer_token_string(tok);
				}
			}
			break;
		}

		if(tok.type != LEXER_TOKEN_NAME) {
			break;
		}

		enum_member_s member = { lexer_token_string(tok) };
		BB_LOG("mm_lexer", "member: %s", member.name.c_str());

		e.members.push_back(member);

		if(!lexer_read_on_line(lex, &tok)) {
			if(!lexer_read(lex, &tok))
				break;
			if(tok.type == LEXER_TOKEN_PUNCTUATION && tok.subtype == LEXER_PUNCT_BRACE_CLOSE) {
				BB_LOG("mm_lexer", "AUTOJSON end enum");
				if(isTypedef) {
					e.typedefBaseName = e.name;
					if(lexer_read(lex, &tok) && tok.type == LEXER_TOKEN_NAME) {
						e.name = lexer_token_string(tok);
					}
				}
			}
			break;
		}

		if(tok.type == LEXER_TOKEN_PUNCTUATION && tok.subtype == LEXER_PUNCT_COMMA) {
			BB_LOG("mm_lexer", "no value");
		} else if(tok.type == LEXER_TOKEN_PUNCTUATION && tok.subtype == LEXER_PUNCT_ASSIGN) {
			BB_LOG("mm_lexer", "value");
			lexer_skip_line(lex);
		} else {
			break;
		}
	}

	if(!e.members.empty()) {
		if(defaultVal.empty()) {
			e.defaultVal = e.members.back().name;
		} else {
			e.defaultVal = defaultVal;
		}
		g_enums.push_back(e);
		return true;
	}
	return false;
}

bool mm_lexer_parse_struct(lexer *lex, bool autovalidate, bool headerOnly, bool fromLoc, bool isTypedef, bool jsonSerialization, bool bStringHash)
{
	lexer_token name;
	if(!lexer_read_on_line(lex, &name)) {
		BB_ERROR("mm_lexer::parse_struct", "Failed to parse 'unknown': expected name on line %u", lex->line);
		return false;
	}

	struct_s s = { lexer_token_string(name), "", autovalidate, headerOnly, fromLoc, jsonSerialization, bStringHash };
	//BB_LOG("mm_lexer", "AUTOJSON struct %s on line %u", s.name.c_str(), name.line);

	lexer_token tok;
	if(!lexer_expect_type(lex, LEXER_TOKEN_PUNCTUATION, LEXER_PUNCT_BRACE_OPEN, &tok)) {
		BB_ERROR("mm_lexer::parse_struct", "Failed to parse '%s': expected { on line %u", s.name.c_str(), lex->line);
		return false;
	}

	int indent = 1;
	while(1) {
		if(!lexer_read(lex, &tok)) {
			BB_ERROR("mm_lexer::parse_struct", "Failed to parse '%s': out of data on line %u", s.name.c_str(), lex->line);
			return false;
		}

		if(tok.type == LEXER_TOKEN_PUNCTUATION && tok.subtype == LEXER_PUNCT_BRACE_CLOSE) {
			--indent;
			if(!indent) {
				if(isTypedef) {
					s.typedefBaseName = s.name;
					if(lexer_read(lex, &tok) && tok.type == LEXER_TOKEN_NAME) {
						s.name = lexer_token_string(tok);
					}
				}
				break;
			}
		} else if(tok.type == LEXER_TOKEN_NAME) {
			std::vector< lexer_token > tokens;
			tokens.push_back(tok);
			size_t equalsIndex = 0;
			size_t bracketIndex = 0;
			while(1) {
				if(!lexer_read(lex, &tok)) {
					BB_ERROR("mm_lexer::parse_struct", "Failed to parse '%s': out of data on line %u", s.name.c_str(), lex->line);
					return false;
				}
				if(tok.type == LEXER_TOKEN_PUNCTUATION && tok.subtype == LEXER_PUNCT_SEMICOLON) {
					break;
				} else {
					if(tok.type == LEXER_TOKEN_PUNCTUATION && tok.subtype == LEXER_PUNCT_ASSIGN) {
						if(!equalsIndex) {
							equalsIndex = tokens.size();
						}
					}
					if(tok.type == LEXER_TOKEN_PUNCTUATION && tok.subtype == LEXER_PUNCT_BRACKET_OPEN) {
						if(!bracketIndex) {
							bracketIndex = tokens.size();
						}
					}
					tokens.push_back(tok);
				}
			}

			if(tokens.size() < 2)
				return false;

			if(equalsIndex > 0 && bracketIndex > 0) {
				BB_ERROR("mm_lexer::parse_struct", "Failed to parse '%s': [] and = cannot be combined on line %u", s.name.c_str(), lex->line);
				return false;
			}

			if(equalsIndex > 1) {
				lexer_token name = tokens[equalsIndex - 1];
				if(name.type != LEXER_TOKEN_NAME) {
					BB_ERROR("mm_lexer::parse_struct", "Failed to parse '%s': expected name on line %u", s.name.c_str(), lex->line);
					return false;
				}

				std::string typestr;
				for(auto i = 0; i < equalsIndex - 1; ++i) {
					typestr += " ";
					typestr += lexer_token_string(tokens[i]);
				}

				std::string valstr;
				for(auto i = equalsIndex + 1; i < tokens.size(); ++i) {
					valstr += " ";
					valstr += lexer_token_string(tokens[i]);
				}

				//BB_LOG("mm_lexer", "member %s:\n  type:%s\n  val:%s", lexer_token_string(name), typestr.c_str(), valstr.c_str());

				struct_member_s m;
				m.name = lexer_token_string(name);
				m.val = valstr[0] == ' ' ? valstr.c_str() + 1 : valstr;
				for(auto i = 0; i < equalsIndex - 1; ++i) {
					m.typeTokens.push_back(tokens[i]);
				}
				s.members.push_back(m);
			} else if(bracketIndex > 1) {
				lexer_token name = tokens[bracketIndex - 1];
				if(name.type != LEXER_TOKEN_NAME) {
					BB_ERROR("mm_lexer::parse_struct", "Failed to parse '%s': expected name on line %u", s.name.c_str(), lex->line);
					return false;
				}

				std::string typestr;
				for(auto i = 0; i < bracketIndex - 1; ++i) {
					typestr += " ";
					typestr += lexer_token_string(tokens[i]);
				}

				std::string valstr;
				for(auto i = bracketIndex + 1; i < tokens.size() - 1; ++i) {
					valstr += " ";
					valstr += lexer_token_string(tokens[i]);
				}

				//BB_LOG("mm_lexer", "member %s:\n  type:%s\n  val:%s", lexer_token_string(name), typestr.c_str(), valstr.c_str());

				struct_member_s m;
				m.name = lexer_token_string(name);
				m.arr = valstr[0] == ' ' ? valstr.c_str() + 1 : valstr;
				for(auto i = 0; i < bracketIndex - 1; ++i) {
					m.typeTokens.push_back(tokens[i]);
				}
				s.members.push_back(m);
			} else if(!equalsIndex) {
				lexer_token name = tokens.back();
				if(name.type != LEXER_TOKEN_NAME) {
					BB_ERROR("mm_lexer::parse_struct", "Failed to parse '%s': expected name on line %u", s.name.c_str(), lex->line);
					return false;
				}

				std::string typestr;
				for(auto i = 0; i < tokens.size() - 1; ++i) {
					typestr += " ";
					typestr += lexer_token_string(tokens[i]);
				}

				//BB_LOG("mm_lexer", "member %s:\n  type:%s", lexer_token_string(name), typestr.c_str());
				struct_member_s m;

				m.name = lexer_token_string(name);
				for(auto i = 0; i < tokens.size() - 1; ++i) {
					m.typeTokens.push_back(tokens[i]);
				}
				s.members.push_back(m);
			}
		} else {
			BB_ERROR("mm_lexer::parse_struct", "Failed to parse '%s': unexpected token on line %u", s.name.c_str(), lex->line);
			return false;
		}
	}

	//BB_LOG("mm_lexer", "struct end");

	std::string sb;
	va(sb, "AUTOJSON ");
	if(isTypedef) {
		va(sb, "typedef ");
	} else {
		va(sb, "struct ");
	}

	if(autovalidate) {
		va(sb, "with AUTOVALIDATE ");
	}
	va(sb, "'%s'\n", s.name.c_str());

	for(auto &m : s.members) {
		std::string ps;
		lexer_token pt = {};
		for(const auto &it : m.typeTokens) {
			std::string s = lexer_token_string(it);
			if(s == ">") {
				m.typeStr += " >";
			} else {
				if(pt.type == LEXER_TOKEN_NAME && it.type == LEXER_TOKEN_NAME) {
					m.typeStr += " ";
				}
				m.typeStr += s;
				if(s == "<") {
					m.typeStr += " ";
				}
			}
			ps = s;
			pt = it;
		}

		if(!m.val.empty()) {
			va(sb, "  %s %s = %s\n", m.typeStr.c_str(), m.name.c_str(), m.val.c_str());
		} else if(!m.arr.empty()) {
			va(sb, "  %s %s[%s]\n", m.typeStr.c_str(), m.name.c_str(), m.arr.c_str());
		} else {
			va(sb, "  %s %s\n", m.typeStr.c_str(), m.name.c_str());
		}
	}
	g_structs.push_back(s);

	BB_LOG("mm_lexer", "%s", sb.c_str());
	return true;
}

static void mm_lexer_scan_file_for_keyword(const char *text, lexer_size text_length, const char *path, const char *basePath, const char *keyword)
{
	/* initialize lexer */
	lexer lex;
	lexer_init(&lex, text, text_length, NULL, NULL, NULL);

	/* parse tokens */

	bool jsonSerialization = !bb_stricmp(keyword, "AUTOJSON");
	bool foundAny = false;
	while(lexer_skip_until(&lex, keyword)) {
		bool headerOnly = g_bHeaderOnly;
		bool isTypedef = lexer_check_string(&lex, "typedef");
		if(lexer_check_string(&lex, "enum")) {
			foundAny = mm_lexer_parse_enum(&lex, "", isTypedef, headerOnly) || foundAny;
		} else if(lexer_check_string(&lex, "AUTODEFAULT")) {
			std::string defaultVal;
			lexer_token tok;
			if(lexer_check_type(&lex, LEXER_TOKEN_PUNCTUATION, LEXER_PUNCT_PARENTHESE_OPEN, &tok)) {
				if(lexer_read(&lex, &tok)) {
					if(tok.type == LEXER_TOKEN_NAME) {
						defaultVal = lexer_token_string(tok);
						if(lexer_check_type(&lex, LEXER_TOKEN_PUNCTUATION, LEXER_PUNCT_PARENTHESE_CLOSE, &tok)) {
							if(lexer_check_string(&lex, "typedef")) {
								isTypedef = true;
							}
							if(lexer_check_string(&lex, "enum")) {
								foundAny = mm_lexer_parse_enum(&lex, defaultVal, isTypedef, headerOnly) || foundAny;
							}
						}
					}
				}
			}
		} else {
			bool autovalidate = false;
			bool fromLoc = false;
			bool bStringHash = false;

			while(1) {
				if(lexer_check_string(&lex, "AUTOVALIDATE")) {
					autovalidate = true;
				} else if(lexer_check_string(&lex, "AUTOHEADERONLY")) {
					headerOnly = true;
				} else if(lexer_check_string(&lex, "AUTOFROMLOC")) {
					fromLoc = true;
				} else if(lexer_check_string(&lex, "AUTOSTRINGHASH")) {
					bStringHash = true;
				} else if(lexer_check_string(&lex, "typedef")) {
					isTypedef = true;
				} else {
					break;
				}
			}

			if(lexer_check_string(&lex, "struct")) {
				foundAny = mm_lexer_parse_struct(&lex, autovalidate, headerOnly, fromLoc, isTypedef, jsonSerialization, bStringHash) || foundAny;
			}
		}
	}

	if(foundAny) {
		size_t basePathLen = strlen(basePath);
		if(basePathLen) {
			++basePathLen;
		}
		g_paths.insert(path + basePathLen);
	}
}

void mm_lexer_scan_file(const char *text, lexer_size text_length, const char *path, const char *basePath)
{
	mm_lexer_scan_file_for_keyword(text, text_length, path, basePath, "AUTOJSON");
	mm_lexer_scan_file_for_keyword(text, text_length, path, basePath, "AUTOSTRUCT");
}

void find_files_in_dir(const char *dir, const char *desiredExt, std::set< std::string > &sd, bool bRecurse)
{
	WIN32_FIND_DATA find;
	HANDLE hFind;

	std::string filter;
	std::string dirPrefix = dir;
	if (*dir) {
		dirPrefix += "\\";
	}
	va(filter, "%s*.*", dirPrefix.c_str());
	if(INVALID_HANDLE_VALUE != (hFind = FindFirstFileA(filter.c_str(), &find))) {
		do {
			if(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if(find.cFileName[0] != '.' && bRecurse) {
					std::string subdir;
					va(subdir, "%s\\%s", dir, find.cFileName);
					if(g_ignorePaths.find(subdir.c_str()) == g_ignorePaths.end()) {
						find_files_in_dir(subdir.c_str(), desiredExt, sd, bRecurse);
					}
				}
			} else {
				const char *ext = strrchr(find.cFileName, '.');
				if(ext && !_stricmp(ext, desiredExt)) {
					sd.insert(ResolvePath(va("%s%s", dirPrefix.c_str(), find.cFileName)));
				}
			}
		} while(FindNextFileA(hFind, &find));
		FindClose(hFind);
	}
}

static void scanHeaders(const std::string &scanDir, bool bRecurse, const std::string &baseDir)
{
	BB_LOG("mm_lexer::scan_dir", "^=%s dir: %s", g_bHeaderOnly ? "header" : "src", scanDir.c_str());
	std::set< std::string > sd;
	find_files_in_dir(scanDir.c_str(), ".h", sd, bRecurse);

	for(const std::string &path : sd) {
		std::string contents = ReadFileContents(path);
		if(contents.empty())
			continue;

		BB_LOG("mm_lexer::scan_file", "^8%s", path.c_str());
		mm_lexer_scan_file(contents.c_str(), (lexer_size)contents.length(), path.c_str(), (!baseDir.empty() && baseDir.length() <= scanDir.length()) ? baseDir.c_str() : scanDir.c_str());
	}
}

void CheckFreeType(const char *outDir)
{
	std::string s;

	std::string freetypePath = va("%s\\..\\submodules\\freetype\\include\\freetype\\freetype.h", outDir);
	freetypePath = ResolvePath(freetypePath);

	va(s, "// Copyright (c) 2012-2022 Matt Campbell\n");
	va(s, "// MIT license (see License.txt)\n");
	va(s, "\n");
	va(s, "// AUTOGENERATED FILE - DO NOT EDIT\n");
	va(s, "\n");
	va(s, "// clang-format off\n");
	va(s, "\n");
	va(s, "#pragma once\n");
	va(s, "\n");
	va(s, "#include \"bb_common.h\"\n");
	va(s, "\n");
	if(bb_file_readable(freetypePath.c_str())) {
		va(s, "#define FEATURE_FREETYPE BB_ON\n");
	} else {
		va(s, "#define FEATURE_FREETYPE BB_OFF\n");
	}

	s = ReplaceChar(s, '\n', "\r\n");
	WriteAndReportFileData(s, outDir, "", "fonts_generated.h");
}

static std::string GetConfigRelativePath(const span_t *configDir, const char *dir)
{
	std::string ret;
	if(configDir->end > configDir->start) {
		va(ret, "%.*s/%s", configDir->end - configDir->start, configDir->start, dir);
	} else {
		ret = dir;
	}
	ret = ResolvePath(ret);
	return ret;
}

static void scanHeaders(span_t *configDir, const preprocInputDir *inputDir)
{
	std::string scanDir = GetConfigRelativePath(configDir, inputDir->dir.c_str());
	std::string baseDir;
	if(inputDir->base.empty()) {
		baseDir = scanDir;
	} else {
		baseDir = GetConfigRelativePath(configDir, inputDir->base.c_str());
	}
	scanHeaders(scanDir, false, baseDir);
}

static void InitLogging(b32 bb, const char *configPath)
{
	if(bb) {
		BB_INIT("mc_common_preproc");
		bb_set_send_callback(&bb_echo_to_stdout, nullptr);
	}

	BB_THREAD_SET_NAME("main");
	BB_LOG("Startup", "Running mc_common_preproc %s", configPath);

	if(!bb) {
		printf("Running mc_common_preproc %s\n", configPath);
	}

	char currentDirectory[1024];
	GetCurrentDirectoryA(sizeof(currentDirectory), currentDirectory);
	BB_LOG("Startup", "Working Directory: %s", currentDirectory);

	TestResolvePath();
}

static void GenerateFromJson(b32 bb, const char *configPath)
{
	span_t configDir = { BB_EMPTY_INITIALIZER };
	configDir.start = configPath;
	configDir.end = GetPathFilename(configPath);
	preprocConfig config = read_preprocConfig(configPath);
	InitLogging(bb || config.bb, GetPathFilename(configPath));

	if(config.input.sourceDirs.empty() && config.input.includeDirs.empty()) {
		BB_ERROR("Config", "Empty config - bailing");
		return;
	}

	g_bHeaderOnly = true;
	for(const preprocInputDir &dir : config.input.includeDirs) {
		scanHeaders(&configDir, &dir);
	}
	g_bHeaderOnly = false;
	for(const preprocInputDir &dir : config.input.sourceDirs) {
		scanHeaders(&configDir, &dir);
	}

	const char *prefix = config.output.prefix.c_str();
	std::string outputSourceDir = GetConfigRelativePath(&configDir, config.output.sourceDir.c_str());
	std::string outputIncludeDir = GetConfigRelativePath(&configDir, config.output.includeDir.c_str());
	std::string outputBaseDir = GetConfigRelativePath(&configDir, config.output.baseDir.c_str());
	std::string includePrefix;

	if(config.output.baseDir.length() < config.output.includeDir.length() &&
	   !bb_strnicmp(outputIncludeDir.c_str(), outputBaseDir.c_str(), outputBaseDir.length())) {
		va(includePrefix, "%s/", outputIncludeDir.c_str() + outputBaseDir.length() + 1);
	}
	GenerateJson(prefix, includePrefix.c_str(), outputSourceDir.c_str(), outputIncludeDir.c_str());
	GenerateStructs(prefix, includePrefix.c_str(), outputSourceDir.c_str(), outputIncludeDir.c_str());
	if(config.input.checkFonts) {
		CheckFreeType(outputIncludeDir.c_str());
	}
}

int CALLBACK main(int argc, const char **argv)
{
	crt_leak_check_init();

	b32 bb = false;
	std::string configPath;
	std::vector< const char * > deferredArgs;
	for(int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if(!bb_stricmp(arg, "-bb")) {
			bb = true;
		} else if(!bb_strnicmp(arg, "-config=", 8)) {
			configPath = arg + 8;
		}
	}

	if(!configPath.empty()) {
		GenerateFromJson(bb, configPath.c_str());
	} else {
		InitLogging(bb, "(null)");
	}

	BB_SHUTDOWN();

	return 0;
}
