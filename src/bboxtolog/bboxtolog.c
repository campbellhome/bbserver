// Copyright (c) 2012-2023 Matt Campbell
// MIT license (see License.txt)

#if defined(_MSC_VER)
__pragma(warning(disable : 4710)); // warning C4710 : 'int printf(const char *const ,...)' : function not inlined
#endif

#include "bb.h"
#include "bbclient/bb_array.h"
#include "bbclient/bb_malloc.h"
#include "bbclient/bb_packet.h"
#include "bbclient/bb_string.h"
#include "bbclient/bb_time.h"
#include "path_utils.h"
#include "sb.h"
#include "span.h"
#include "tokenize.h"
#include "va.h"

#include "recorded_session.h"
#include "view.h"
#include "view_filter/view_filter.h"

#include "bb_wrap_dirent.h"
#include "bbclient/bb_wrap_malloc.h"
#include "bbclient/bb_wrap_stdio.h"
#include <stdlib.h>
#include <string.h>

#if !defined(BB_NO_SQLITE)
#include <sqlite/wrap_sqlite3.h>
#endif

BB_WARNING_DISABLE(5045);

// BB_WARNING_DISABLE(4464); // warning C4464: relative include path contains '..'
// #include "../view_filter.h"

typedef enum tag_program
{
	kProgram_bboxtolog,
	kProgram_bbcat,
	kProgram_bbtail,
	kProgram_bbgrep,
} program;

typedef enum tag_exitCode
{
	kExitCode_Success,
	kExitCode_Error_Usage,
	kExitCode_Error_ReadSource,
	kExitCode_Error_WriteTarget,
	kExitCode_Error_Decode,
} exitCode;

typedef struct category_s
{
	u32 id;
	char name[64];
} category_t;

typedef struct categories_s
{
	category_t* data;
	u32 count;
	u32 allocated;
} categories_t;
view_category_t s_viewCategory;

typedef struct logPacket_s
{
	s64 ms;
	sb_t lines;
	u32 categoryId;
	u8 pad[4];
} logPacket_t;

typedef struct logPackets_s
{
	u32 count;
	u32 allocated;
	logPacket_t* data;
} logPackets_t;

static char* g_exe;
static program g_program;
static u8 g_recvBuffer[1 * 1024 * 1024];

static partial_logs_t g_partialLogs;
static b32 g_inTailCatchup;
static b32 g_follow;
static u32 g_numLines;
static bb_log_level_e g_verbosity;

categories_t g_categories = { 0, 0, 0 };
double g_millisPerTick = 1.0;
u64 g_initialTimestamp = 0;
static logPackets_t g_queuedPackets;
static const char* g_pathToPrint;

#if !defined(BB_NO_SQLITE)
static sqlite3* db;
#endif
static const char* g_sqlCommand;

static void print_stderr(const char* text)
{
	fputs(text, stderr);
#if BB_USING(BB_PLATFORM_WINDOWS)
	OutputDebugStringA(text);
#endif
}

static int usage(void)
{
	if (g_program == kProgram_bboxtolog)
	{
		print_stderr(va("Usage: %s filename.bbox <filename.log>\n", g_exe));
		print_stderr(va("If no output filename is specified, the target will be the source with .bbox\nextension replaced with .log\n"));
	}
	else
	{
		print_stderr(va("Usage: %s filename.bbox\n", g_exe));
	}
	return kExitCode_Error_Usage;
}

static inline const char* get_category(u32 categoryId)
{
	const char* category = "";
	for (u32 i = 0; i < g_categories.count; ++i)
	{
		category_t* c = g_categories.data + i;
		if (c->id == categoryId)
		{
			category = c->name;
			break;
		}
	}
	return category;
}

char* recorded_session_get_thread_name(recorded_session_t* session, u64 threadId)
{
	BB_UNUSED(session);
	BB_UNUSED(threadId);
	return "";
}

const char* recorded_session_get_filename(recorded_session_t* session, u32 fileId)
{
	BB_UNUSED(session);
	BB_UNUSED(fileId);
	return "";
}

const char* recorded_session_get_category_name(recorded_session_t* session, u32 categoryId)
{
	BB_UNUSED(session);
	return get_category(categoryId);
}

view_category_t* view_find_category(view_t* view, u32 categoryId)
{
	BB_UNUSED(view);
	for (u32 i = 0; i < g_categories.count; ++i)
	{
		category_t* category = g_categories.data + i;
		if (category->id == categoryId)
		{
			bb_strncpy(s_viewCategory.categoryName, category->name, sizeof(s_viewCategory.categoryName));
			return &s_viewCategory;
		}
	}
	return NULL;
}

static void print_lines(logPacket_t packet, FILE* ofp)
{
	const char* category = get_category(packet.categoryId);
	const char* lines = sb_get(&packet.lines);
	span_t linesCursor = span_from_string(lines);
	for (span_t line = tokenizeLine(&linesCursor); line.start; line = tokenizeLine(&linesCursor))
	{
		sb_t text;
		if (g_pathToPrint)
		{
			text = sb_from_va("%s: [%7lld] [%13s] %.*s\n", g_pathToPrint, packet.ms, category, (int)(line.end - line.start), line.start);
		}
		else
		{
			text = sb_from_va("[%7lld] [%13s] %.*s\n", packet.ms, category, (int)(line.end - line.start), line.start);
		}
		fputs(sb_get(&text), ofp);
#if BB_USING(BB_PLATFORM_WINDOWS)
		OutputDebugStringA(sb_get(&text));
#endif
		sb_reset(&text);
	}
}

static void reset_logPacket_t(logPacket_t* packet)
{
	sb_reset(&packet->lines);
}

static void queue_lines(sb_t lines, u32 categoryId, s64 ms, FILE* ofp)
{
	logPacket_t packet = { BB_EMPTY_INITIALIZER };
	packet.ms = ms;
	packet.lines = lines;
	packet.categoryId = categoryId;

	if (g_inTailCatchup)
	{
		if (g_queuedPackets.count >= g_numLines)
		{
			reset_logPacket_t(g_queuedPackets.data);
			bba_erase(g_queuedPackets, 0);
		}
		bba_push(g_queuedPackets, packet);
	}
	else
	{
		print_lines(packet, ofp);
		reset_logPacket_t(&packet);
	}
}

static s32 get_verbosity_sort(bb_log_level_e verbosity)
{
	switch (verbosity)
	{
	case kBBLogLevel_VeryVerbose: return 1;
	case kBBLogLevel_Verbose: return 2;
	case kBBLogLevel_Log: return 3;
	case kBBLogLevel_Display: return 4;
	case kBBLogLevel_Warning: return 5;
	case kBBLogLevel_Error: return 6;
	case kBBLogLevel_Fatal: return 7;
	case kBBLogLevel_SetColor:
	case kBBLogLevel_Count:
	default: return 0;
	}
}
static b32 test_verbosity(const bb_decoded_packet_t* decoded)
{
	bb_log_level_e verbosity = decoded->packet.logText.level;
	return (get_verbosity_sort(verbosity) >= get_verbosity_sort(g_verbosity));
}

#if !defined(BB_NO_SQLITE)
static b32 sqlite3_bind_u32_and_log(sqlite3_stmt* stmt, int index, u32 value)
{
	int rc = sqlite3_bind_int(stmt, index, value);
	if (rc == SQLITE_OK)
	{
		return true;
	}
	else
	{
		fprintf(stderr, "SQL bind error: index:%d value:%u result:%d error:%s\n", index, value, rc, sqlite3_errmsg(db));
		return false;
	}
}

static b32 sqlite3_bind_text_and_log(sqlite3_stmt* stmt, int index, const char* value)
{
	int rc = sqlite3_bind_text(stmt, index, value, -1, SQLITE_STATIC);
	if (rc == SQLITE_OK)
	{
		return true;
	}
	else
	{
		fprintf(stderr, "SQL bind error: index:%d value:%s result:%d error:%s\n", index, value, rc, sqlite3_errmsg(db));
		return false;
	}
}

static int test_sql_callback(void* userData, int argc, char** argv, char** colNames)
{
	BB_UNUSED(argc);
	BB_UNUSED(argv);
	BB_UNUSED(colNames);
	int* count = (int*)userData;
	*count += 1;
	return 0;
}

static b32 test_sql_internal(const bb_decoded_packet_t* decoded, const char* lines)
{
	// "(line INTEGER PRIMARY KEY, category TEXT, level TEXT, pie NUMBER, text TEXT);";

	const char* category = get_category(decoded->packet.logText.categoryId);
	const char* level = bb_get_log_level_name(decoded->packet.logText.level, "Unknown");

	int rc;
	const char* insert_sql = "INSERT INTO logs (line, category, level, pie, text) VALUES(?, ?, ?, ?, ?)";
	sqlite3_stmt* insert_stmt = NULL;
	rc = sqlite3_prepare_v2(db, insert_sql, -1, &insert_stmt, NULL);
	if (rc != SQLITE_OK)
	{
		return false;
	}

	u32 line = 0;
	sqlite3_bind_u32_and_log(insert_stmt, 1, line);
	sqlite3_bind_text_and_log(insert_stmt, 2, category);
	sqlite3_bind_text_and_log(insert_stmt, 3, level);
	sqlite3_bind_u32_and_log(insert_stmt, 4, decoded->packet.logText.pieInstance);
	sqlite3_bind_text_and_log(insert_stmt, 5, lines);

	rc = sqlite3_step(insert_stmt);
	if (SQLITE_DONE != rc)
	{
		fprintf(stderr, "SQL error running INSERT INTO: result:%d error:%s\n", rc, sqlite3_errmsg(db));
	}
	sqlite3_clear_bindings(insert_stmt);
	sqlite3_reset(insert_stmt);
	sqlite3_finalize(insert_stmt);

	int count = 0;
	char* errorMessage = NULL;
	rc = sqlite3_exec(db, g_sqlCommand, test_sql_callback, &count, &errorMessage);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL error running user cmd: %d %s\n", rc, errorMessage);
		sqlite3_free(errorMessage);
	}

	return count > 0;
}
#endif // #if !defined(BB_NO_SQLITE)

static b32 test_sql(const bb_decoded_packet_t* decoded, const char* lines)
{
#if defined(BB_NO_SQLITE)
	BB_UNUSED(decoded);
	BB_UNUSED(lines);
	return true;
#else
	if (!g_sqlCommand || !db)
		return true;

	int rc;
	char* errorMessage;

	rc = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &errorMessage);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL error running BEGIN TRANSACTION ret:%d error:%s\n", rc, errorMessage);
		sqlite3_free(errorMessage);
		return false;
	}

	b32 result = test_sql_internal(decoded, lines);

	rc = sqlite3_exec(db, "ROLLBACK", NULL, NULL, &errorMessage);
	if (rc != SQLITE_OK)
	{
		BB_ERROR("sqlite", "SQL error running ROLLBACK ret:%d error:%s\n", rc, errorMessage);
		sqlite3_free(errorMessage);
	}

	return result;
#endif
}

typedef struct vfilter_data_s
{
	view_t view;
	recorded_log_t recordedLog;
} vfilter_data_t;

static b32 test_vfilter(vfilter_data_t* vfilter_data)
{
	if (vfilter_data)
	{
		return view_filter_visible(&vfilter_data->view, &vfilter_data->recordedLog);
	}
	return true;
}

static void queue_packet(const bb_decoded_packet_t* decoded, FILE* ofp, vfilter_data_t* vfilter_data)
{
	sb_t lines = { BB_EMPTY_INITIALIZER };
	for (u32 i = 0; i < g_partialLogs.count; ++i)
	{
		const bb_decoded_packet_t* partial = g_partialLogs.data + i;
		if (partial->header.threadId == decoded->header.threadId)
		{
			sb_append(&lines, partial->packet.logText.text);
		}
	}
	sb_append(&lines, decoded->packet.logText.text);

	s64 ms = (s64)((decoded->header.timestamp - g_initialTimestamp) * g_millisPerTick);
	if (test_verbosity(decoded) && test_sql(decoded, sb_get(&lines)) && test_vfilter(vfilter_data))
	{
		queue_lines(lines, decoded->packet.logText.categoryId, ms, ofp);
	}
	else
	{
		sb_reset(&lines);
	}

	for (u32 i = 0; i < g_partialLogs.count;)
	{
		const bb_decoded_packet_t* partial = g_partialLogs.data + i;
		if (partial->header.threadId == decoded->header.threadId)
		{
			bba_erase(g_partialLogs, i);
		}
		else
		{
			++i;
		}
	}
}

static void print_queued(FILE* ofp)
{
	for (u32 i = 0; i < g_queuedPackets.count; ++i)
	{
		logPacket_t* packet = g_queuedPackets.data + i;
		print_lines(*packet, ofp);
	}
}

// static b32 view_filter_visible(vfilter_t* vfilter, recorded_log_t* log)
//{
//	if (!vfilter->valid || !vfilter->tokens.count)
//		return true;
//
//	switch (vfilter->type)
//	{
//	case kVF_Standard:
//		return view_filter_visible_standard(view, log);
//	case kVF_SQL:
//		return true;
//	case kVF_Legacy:
//		return view_filter_visible_legacy(view, log);
//	case kVF_Count:
//	default:
//		return true;
//	}
// }

static void bbgrep_file(const sb_t* path, const char* filter)
{
	vfilter_data_t vfilter_data = { BB_EMPTY_INITIALIZER };
	vfilter_data.view.vfilter = view_filter_parse("unnamed", filter);
	vfilter_data.view.config.filterActive = true;

	g_pathToPrint = sb_get(path);
	FILE* fp = fopen(g_pathToPrint, "rb");
	if (fp)
	{
		u32 recvCursor = 0;
		u32 decodeCursor = 0;
		b32 done = false;
		while (!done)
		{
			if (recvCursor < sizeof(g_recvBuffer))
			{
				size_t bytesRead = fread(g_recvBuffer + recvCursor, 1, sizeof(g_recvBuffer) - recvCursor, fp);
				if (bytesRead)
				{
					recvCursor += (u32)bytesRead;
				}
			}
			const u32 krecvBufferSize = sizeof(g_recvBuffer);
			const u32 kHalfrecvBufferBytes = krecvBufferSize / 2;
			bb_decoded_packet_t decoded;
			u32 nDecodableBytes32 = recvCursor - decodeCursor;
			u16 nDecodableBytes = nDecodableBytes32 > 0xFFFF ? 0xFFFF : (u16)(nDecodableBytes32);
			u8* cursor = g_recvBuffer + decodeCursor;
			u16 nPacketBytes = (nDecodableBytes >= 3) ? (*cursor << 8) + (*(cursor + 1)) : 0;
			if (nPacketBytes == 0 || nPacketBytes > nDecodableBytes)
			{
				done = true;
				break;
			}

			if (bbpacket_deserialize(cursor + 2, nPacketBytes - 2, &decoded))
			{
				if (bbpacket_is_app_info_type(decoded.type))
				{
					g_initialTimestamp = decoded.packet.appInfo.initialTimestamp;
					g_millisPerTick = decoded.packet.appInfo.millisPerTick;
				}
				else
				{
					BB_WARNING_PUSH(4061); // warning C4061: enumerator 'kBBPacketType_Invalid' in switch of enum 'bb_packet_type_e' is not explicitly handled by a case label
					switch (decoded.type)
					{
					case kBBPacketType_CategoryId:
					{
						category_t* c = bba_add(g_categories, 1);
						if (c)
						{
							const char* temp;
							const char* category = decoded.packet.categoryId.name;
							while ((temp = strstr(category, "::")) != NULL)
							{
								if (*temp)
								{
									category = temp + 2;
								}
								else
								{
									break;
								}
							}
							c->id = decoded.packet.categoryId.id;
							bb_strncpy(c->name, category, sizeof(c->name));
						}
						break;
					}
					case kBBPacketType_LogTextPartial:
					{
						bba_push(g_partialLogs, decoded);
						break;
					}
					case kBBPacketType_LogText_v1:
					case kBBPacketType_LogText_v2:
					case kBBPacketType_LogText:
					{
						vfilter_data.recordedLog.packet = decoded;
						vfilter_data.recordedLog.numLines = 0;
						for (u32 i = 0; i < g_partialLogs.count; ++i)
						{
							const bb_decoded_packet_t* partial = g_partialLogs.data + i;
							if (partial->header.threadId == decoded.header.threadId)
							{
								++vfilter_data.recordedLog.numLines;
							}
						}
						queue_packet(&decoded, stdout, &vfilter_data);
						vfilter_data.recordedLog.sessionLogIndex++;
						break;
					}
					default:
						break;
					}
					BB_WARNING_POP;
				}
			}
			else
			{
				fprintf(stderr, "Failed to decode packet from %s\n", g_pathToPrint);
				done = true;
			}

			decodeCursor += nPacketBytes;

			// TODO: rather lame to keep resetting the buffer - this should be a circular buffer
			if (decodeCursor >= kHalfrecvBufferBytes)
			{
				u32 nBytesRemaining = recvCursor - decodeCursor;
				memmove(g_recvBuffer, g_recvBuffer + decodeCursor, nBytesRemaining);
				decodeCursor = 0;
				recvCursor = nBytesRemaining;
			}
		}

		fclose(fp);
		bba_free(g_partialLogs);
		bba_free(g_categories);
	}
	else
	{
		fprintf(stderr, "Could not read %s\n", g_pathToPrint);
	}

	g_pathToPrint = NULL;
	vfilter_reset(&vfilter_data.view.vfilter);
}

static void separateFilename(const char* filename, sb_t* base, sb_t* ext)
{
	const char* separator = strrchr(filename, '.');
	if (separator)
	{
		span_t baseSpan = { filename, separator };
		*base = sb_from_span(baseSpan);
		*ext = sb_from_c_string(separator + 1);
	}
	else
	{
		*base = sb_from_c_string(filename);
	}
}

static b32 wildcardMatch(const char* pattern, const char* input)
{
	// skip UE backups for now
	if (bb_stristr(input, "-backup-") != NULL)
		return false;

	// only support * or full match for now
	if (!*pattern || bb_stricmp(pattern, "*") == 0)
		return true;

	return bb_stristr(input, pattern) != NULL;
}

static int bbgrep(const char* filter, sb_t* dirName, const char* pattern, b32 bRecursive)
{
	sbs_t files = { BB_EMPTY_INITIALIZER };
	sbs_t dirs = { BB_EMPTY_INITIALIZER };
	DIR* d = opendir(sb_get(dirName));
	if (d)
	{
		sb_t patternFilename = { BB_EMPTY_INITIALIZER };
		sb_t patternExtension = { BB_EMPTY_INITIALIZER };
		separateFilename(pattern, &patternFilename, &patternExtension);

		struct dirent* entry;
		while ((entry = readdir(d)) != NULL)
		{
			if (entry->d_type == DT_REG)
			{
				sb_t entryFilename = { BB_EMPTY_INITIALIZER };
				sb_t entryExtension = { BB_EMPTY_INITIALIZER };
				separateFilename(entry->d_name, &entryFilename, &entryExtension);

				// only support * or full match for now
				if (wildcardMatch(sb_get(&patternFilename), sb_get(&entryFilename)))
				{
					if (wildcardMatch(sb_get(&patternExtension), sb_get(&entryExtension)))
					{
						sb_t* val = bba_add(files, 1);
						if (val)
						{
							*val = sb_from_va("%s\\%s", sb_get(dirName), entry->d_name);
						}
					}
				}

				sb_reset(&entryFilename);
				sb_reset(&entryExtension);
			}
		}
		closedir(d);

		sb_reset(&patternFilename);
		sb_reset(&patternExtension);
	}

	for (u32 i = 0; i < files.count; ++i)
	{
		bbgrep_file(files.data + i, filter);
	}

	if (bRecursive)
	{
		d = opendir(sb_get(dirName));
		if (d)
		{
			struct dirent* entry;
			while ((entry = readdir(d)) != NULL)
			{
				if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
				{
					sb_t* val = bba_add(dirs, 1);
					if (val)
					{
						*val = sb_from_va("%s\\%s", sb_get(dirName), entry->d_name);
					}
				}
			}
			closedir(d);
		}

		for (u32 i = 0; i < dirs.count; ++i)
		{
			sb_t* subdir = dirs.data + i;
			bbgrep(filter, subdir, pattern, bRecursive);
		}
	}

	sbs_reset(&files);
	sbs_reset(&dirs);

	return 0;
}

int main_loop(int argc, char** argv)
{
	g_exe = argv[0];
	char* sep = strrchr(g_exe, '\\');
	if (sep)
	{
		g_exe = sep + 1;
	}
	sep = strrchr(g_exe, '/');
	if (sep)
	{
		g_exe = sep + 1;
	}

	sep = strrchr(g_exe, '.');
	if (sep)
	{
		*sep = 0;
	}

	if (!bb_stricmp(g_exe, "bbcat"))
	{
		g_program = kProgram_bbcat;
	}
	else if (!bb_stricmp(g_exe, "bbtail"))
	{
		g_program = kProgram_bbtail;
	}
	else if (!bb_stricmp(g_exe, "bbgrep"))
	{
		g_program = kProgram_bbgrep;
	}

	const char* source = NULL;
	char* target = NULL;
	b32 bRecursive = false;
	b32 bPastSwitches = false;
	for (int i = 1; i < argc; ++i)
	{
		const char* arg = argv[i];
		if (!strcmp(arg, "--"))
		{
			bPastSwitches = true;
			continue;
		}

		if (!bPastSwitches && *arg == '-')
		{
			if (!strcmp(arg, "-f"))
			{
				g_follow = true;
			}
			else if (!strcmp(arg, "-n"))
			{
				if (i + 1 < argc)
				{
					g_numLines = atoi(argv[i + 1]);
					++i;
				}
				else
				{
					return usage();
				}
			}
			else if (!strcmp(arg, "-v") || !strcmp(arg, "-verbosity"))
			{
				if (i + 1 < argc)
				{
					++i;
					arg = argv[i];
					if (!bb_stricmp(arg, "log"))
					{
						g_verbosity = kBBLogLevel_Log;
					}
					else if (!bb_stricmp(arg, "warning"))
					{
						g_verbosity = kBBLogLevel_Warning;
					}
					else if (!bb_stricmp(arg, "error"))
					{
						g_verbosity = kBBLogLevel_Error;
					}
					else if (!bb_stricmp(arg, "display"))
					{
						g_verbosity = kBBLogLevel_Display;
					}
					else if (!bb_stricmp(arg, "veryverbose"))
					{
						g_verbosity = kBBLogLevel_VeryVerbose;
					}
					else if (!bb_stricmp(arg, "verbose"))
					{
						g_verbosity = kBBLogLevel_Verbose;
					}
					else if (!bb_stricmp(arg, "fatal"))
					{
						g_verbosity = kBBLogLevel_Fatal;
					}
					else
					{
						return usage();
					}
				}
				else
				{
					return usage();
				}
			}
			else if (!strcmp(arg, "-bbcat"))
			{
				g_program = kProgram_bbcat;
			}
			else if (!strcmp(arg, "-bbtail"))
			{
				g_program = kProgram_bbtail;
			}
			else if (!strcmp(arg, "-bbgrep"))
			{
				g_program = kProgram_bbgrep;
			}
			else if (!bb_strnicmp(arg, "-sql=", 5))
			{
				g_sqlCommand = arg + 5;
			}
			else if (!strcmp(arg, "-r"))
			{
				bRecursive = true;
			}
			else
			{
				return usage();
			}
		}
		else
		{
			if (!source)
			{
				source = arg;
			}
			else if (!target)
			{
				target = bb_strdup(arg);
			}
			else
			{
				return usage();
			}
		}
	}

	if (!source)
		return usage(); // TODO: read stdin

	if (target && g_program != kProgram_bboxtolog && g_program != kProgram_bbgrep)
		return usage();

	if (g_follow && g_program == kProgram_bboxtolog)
		return usage();

	if (g_program == kProgram_bbtail)
	{
		g_inTailCatchup = true;
	}

	if (g_numLines == 0)
	{
		g_numLines = 10;
	}

	if (g_program == kProgram_bbgrep)
	{
		const char* filename = "*";
		sb_t dir = sb_from_c_string(target);
		const char* ext = bb_stristr(sb_get(&dir), ".bbox");
		if (ext)
		{
			ptrdiff_t offset = ext - dir.data;
			if (offset == dir.count - 6)
			{
				filename = path_get_filename(target);
				path_remove_filename(&dir);
			}
		}
		if (sb_len(&dir) == 0)
		{
			sb_append(&dir, ".");
		}
		return bbgrep(source, &dir, filename, bRecursive);
	}

	size_t sourceLen = strlen(source);
	if (sourceLen < 6 || bb_stricmp(source + sourceLen - 5, ".bbox"))
	{
		return usage();
	}

	if (!target && (g_program == kProgram_bboxtolog))
	{
		target = bb_strdup(source);
		strcpy(target + sourceLen - 5, ".log");
	}

	// printf("source: %s\n", source);
	// if(target) {
	//	printf("target: %s\n", target);
	// }

	int ret = kExitCode_Success;

	FILE* fp = fopen(source, "rb");
	if (fp)
	{
		FILE* ofp = (target) ? fopen(target, "wt") : stdout;
		if (ofp)
		{

			u32 recvCursor = 0;
			u32 decodeCursor = 0;
			b32 done = false;
			while (!done)
			{
				if (recvCursor < sizeof(g_recvBuffer))
				{
					size_t bytesRead = fread(g_recvBuffer + recvCursor, 1, sizeof(g_recvBuffer) - recvCursor, fp);
					if (bytesRead)
					{
						recvCursor += (u32)bytesRead;
					}
				}
				const u32 krecvBufferSize = sizeof(g_recvBuffer);
				const u32 kHalfrecvBufferBytes = krecvBufferSize / 2;
				bb_decoded_packet_t decoded;
				u32 nDecodableBytes32 = recvCursor - decodeCursor;
				u16 nDecodableBytes = nDecodableBytes32 > 0xFFFF ? 0xFFFF : (u16)(nDecodableBytes32);
				u8* cursor = g_recvBuffer + decodeCursor;
				u16 nPacketBytes = (nDecodableBytes >= 3) ? (*cursor << 8) + (*(cursor + 1)) : 0;
				if (nPacketBytes == 0 || nPacketBytes > nDecodableBytes)
				{
					if (g_program == kProgram_bbtail)
					{
						if (g_inTailCatchup)
						{
							print_queued(ofp);
							g_inTailCatchup = false;
						}
					}

					if (g_follow)
					{
						bb_sleep_ms(10);
						continue;
					}
					else
					{
						done = true;
						break;
					}
				}

				if (bbpacket_deserialize(cursor + 2, nPacketBytes - 2, &decoded))
				{
					if (bbpacket_is_app_info_type(decoded.type))
					{
						g_initialTimestamp = decoded.packet.appInfo.initialTimestamp;
						g_millisPerTick = decoded.packet.appInfo.millisPerTick;
					}
					else
					{
						BB_WARNING_PUSH(4061); // warning C4061: enumerator 'kBBPacketType_Invalid' in switch of enum 'bb_packet_type_e' is not explicitly handled by a case label
						switch (decoded.type)
						{
						case kBBPacketType_CategoryId:
						{
							category_t* c = bba_add(g_categories, 1);
							if (c)
							{
								const char* temp;
								const char* category = decoded.packet.categoryId.name;
								while ((temp = strstr(category, "::")) != NULL)
								{
									if (*temp)
									{
										category = temp + 2;
									}
									else
									{
										break;
									}
								}
								c->id = decoded.packet.categoryId.id;
								bb_strncpy(c->name, category, sizeof(c->name));
							}
							break;
						}
						case kBBPacketType_LogTextPartial:
						{
							bba_push(g_partialLogs, decoded);
							break;
						}
						case kBBPacketType_LogText_v1:
						case kBBPacketType_LogText_v2:
						case kBBPacketType_LogText:
						{
							queue_packet(&decoded, ofp, NULL);
							break;
						}
						default:
							break;
						}
						BB_WARNING_POP;
					}
				}
				else
				{
					fprintf(stderr, "Failed to decode packet from %s\n", source);
					ret = kExitCode_Error_Decode;
					done = true;
				}

				decodeCursor += nPacketBytes;

				// TODO: rather lame to keep resetting the buffer - this should be a circular buffer
				if (decodeCursor >= kHalfrecvBufferBytes)
				{
					u32 nBytesRemaining = recvCursor - decodeCursor;
					memmove(g_recvBuffer, g_recvBuffer + decodeCursor, nBytesRemaining);
					decodeCursor = 0;
					recvCursor = nBytesRemaining;
				}
			}

			fclose(fp);
			fclose(ofp);
			bba_free(g_categories);
		}
		else
		{
			if (target)
			{
				fprintf(stderr, "Could not write to %s\n", target);
			}
			fclose(fp);
			ret = kExitCode_Error_WriteTarget;
		}
	}
	else
	{
		fprintf(stderr, "Could not read %s\n", source);
		ret = kExitCode_Error_ReadSource;
	}

	if (target)
	{
		bb_free(target);
	}

	return ret;
}

int main(int argc, char** argv)
{
#if defined(BB_NO_SQLITE)
	return main_loop(argc, argv);
#else
	int rc = sqlite3_open(":memory:", &db);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "Could not open in-memory database\n");
		if (db)
		{
			sqlite3_close(db);
			db = NULL;
		}
	}

	if (db)
	{
		const char* createCommand = "CREATE TABLE logs (line INTEGER PRIMARY KEY, category TEXT, level TEXT, pie NUMBER, text TEXT);";
		char* errorMessage = NULL;
		rc = sqlite3_exec(db, createCommand, NULL, NULL, &errorMessage);
		if (rc != SQLITE_OK)
		{
			BB_ERROR("sqlite", "SQL error running CREATE TABLE: %s", errorMessage);
			sqlite3_free(errorMessage);
			sqlite3_close(db);
			db = NULL;
		}
	}

	int ret = main_loop(argc, argv);

	if (db)
	{
		sqlite3_close(db);
		db = NULL;
	}
	return ret;
#endif
}
