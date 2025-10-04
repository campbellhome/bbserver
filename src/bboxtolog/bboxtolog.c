// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#if defined(_MSC_VER)
__pragma(warning(disable : 4710)); // warning C4710 : 'int printf(const char *const ,...)' : function not inlined
#endif

#include "bb.h"
#include "bbstats.h"
#include "bbclient/bb_array.h"
#include "bbclient/bb_file.h"
#include "bbclient/bb_malloc.h"
#include "bbclient/bb_packet.h"
#include "bbclient/bb_string.h"
#include "bbclient/bb_time.h"
#include "bboxtolog_utils.h"
#include "crt_leak_check.h"
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

#include <parson/parson.h>

#if !defined(BB_NO_SQLITE)
#include <sqlite/wrap_sqlite3.h>
#endif

#if BB_USING(BB_COMPILER_MSVC)
#include <locale.h>
#endif

BB_WARNING_DISABLE(5045);

typedef enum tag_program
{
	kProgram_bboxtolog,
	kProgram_bbcat,
	kProgram_bbtail,
	kProgram_bbgrep,
	kProgram_bboxtojson,
	kProgram_bbstats,
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
	bb_log_level_e logLevel;
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
static b32 g_printFilename;

typedef enum plaintext_prefix_e
{
	kPlaintextPrefix_Auto,
	kPlaintextPrefix_None,
	kPlaintextPrefix_Unreal,
	kPlaintextPrefix_Brackets,
} plaintext_prefix_t;
static plaintext_prefix_t s_plaintext_prefix;

categories_t g_categories = { 0, 0, 0 };
double g_millisPerTick = 1.0;
u64 g_initialTimestamp = 0;
static logPackets_t g_queuedPackets;
static const char* g_pathToPrint;

#if !defined(BB_NO_SQLITE)
static sqlite3* db;
#endif
static const char* g_sqlCommand;

void print_stderr(const char* text)
{
	fputs(text, stderr);
#if BB_USING(BB_PLATFORM_WINDOWS)
	OutputDebugStringA(text);
#endif
}

void print_stdout(const char* text)
{
	fputs(text, stdout);
#if BB_USING(BB_PLATFORM_WINDOWS)
	OutputDebugStringA(text);
#endif
}

void print_fp(const char* text, FILE *fp)
{
	fputs(text, fp);
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
	else if (g_program == kProgram_bboxtojson)
	{
		print_stderr(va("Usage: %s filename.bbox <filename.json>\n", g_exe));
		print_stderr(va("If no output filename is specified, the target will be the source with .bbox\nextension replaced with .json\n"));
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

static inline u32 find_or_add_category_by_name(const char* category)
{
	for (u32 i = 0; i < g_categories.count; ++i)
	{
		category_t* c = g_categories.data + i;
		if (!bb_stricmp(c->name, category))
		{
			return c->id;
		}
	}

	category_t* c = bba_add(g_categories, 1);
	if (c)
	{
		c->id = g_categories.count;
		bb_strncpy(c->name, category, sizeof(c->name));
		return c->id;
	}

	return 0;
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
		const char* filename = (g_pathToPrint && g_printFilename) ? va("%s: ", g_pathToPrint) : "";
		const char* logLevel = (packet.logLevel != kBBLogLevel_Log) ? va("[%s]", bb_get_log_level_name(packet.logLevel, "")) : "";
		sb_t text = sb_from_va("%s[%lld][%s]%s %.*s\n", filename, packet.ms, category, logLevel, (int)(line.end - line.start), line.start);
		print_fp(sb_get(&text), ofp);
		sb_reset(&text);
	}
}

static void reset_logPacket_t(logPacket_t* packet)
{
	sb_reset(&packet->lines);
}

static void queue_lines(sb_t lines, u32 categoryId, bb_log_level_e logLevel, s64 ms, FILE* ofp)
{
	logPacket_t packet = { BB_EMPTY_INITIALIZER };
	packet.ms = ms;
	packet.lines = lines;
	packet.categoryId = categoryId;
	packet.logLevel = logLevel;

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

process_packet_data_t build_packet_data(const bb_decoded_packet_t* decoded)
{
	process_packet_data_t data = { BB_EMPTY_INITIALIZER };
	data.threadId = decoded->header.threadId;
	data.milliseconds = (s64)((decoded->header.timestamp - g_initialTimestamp) * g_millisPerTick);

	for (u32 i = 0; i < g_partialLogs.count; ++i)
	{
		const bb_decoded_packet_t* partial = g_partialLogs.data + i;
		if (partial->header.threadId == decoded->header.threadId)
		{
			sb_append(&data.lines, partial->packet.logText.text);
			++data.numPartialLogsUsed;
		}
	}
	sb_append(&data.lines, decoded->packet.logText.text);

	return data;
}

void reset_packet_data(process_packet_data_t *data)
{
	if (data->numPartialLogsUsed)
	{
		u32 numPartialLogs = g_partialLogs.count;
		for (u32 reverseIndex = 0; reverseIndex < numPartialLogs; ++reverseIndex)
		{
			u32 i = numPartialLogs - reverseIndex - 1;
			const bb_decoded_packet_t* partial = g_partialLogs.data + i;
			if (partial->header.threadId == data->threadId)
			{
				bba_erase(g_partialLogs, i);
			}
		}
	}
	sb_reset(&data->lines);
}

static void queue_packet(const bb_decoded_packet_t* decoded, FILE* ofp, vfilter_data_t* vfilter_data)
{
	process_packet_data_t packetData = build_packet_data(decoded);

	if (test_verbosity(decoded) && test_sql(decoded, sb_get(&packetData.lines)) && test_vfilter(vfilter_data))
	{
		queue_lines(packetData.lines, decoded->packet.logText.categoryId, decoded->packet.logText.level, packetData.milliseconds, ofp);
		
		// ownership of packetData.lines transferred, so clear out packetData.lines to avoid reset
		sb_t empty = { BB_EMPTY_INITIALIZER };
		packetData.lines = empty;
	}

	reset_packet_data(&packetData);
}

static void print_queued(FILE* ofp)
{
	for (u32 i = 0; i < g_queuedPackets.count; ++i)
	{
		logPacket_t* packet = g_queuedPackets.data + i;
		print_lines(*packet, ofp);
	}
}

static int process_bbox_file(process_file_data_t* process_file_data)
{
	int ret = kExitCode_Success;

	FILE* fp = fopen(process_file_data->source, "rb");
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
				if (g_program == kProgram_bbtail)
				{
					if (g_inTailCatchup)
					{
						if (process_file_data->tail_catchup_func)
						{
							(*process_file_data->tail_catchup_func)(process_file_data);
						}
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
				if (g_program == kProgram_bboxtojson)
				{
					if (process_file_data->packet_func)
					{
						(*process_file_data->packet_func)(&decoded, process_file_data);
					}
				}
				else if (bbpacket_is_app_info_type(decoded.type))
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
							c->id = decoded.packet.categoryId.id;
							bb_strncpy(c->name, decoded.packet.categoryId.name, sizeof(c->name));
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
						if (process_file_data->packet_func)
						{
							(*process_file_data->packet_func)(&decoded, process_file_data);
						}
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
				fprintf(stderr, "Failed to decode packet from %s\n", process_file_data->source);
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
		bba_free(g_categories);
	}
	else
	{
		fprintf(stderr, "Could not read %s\n", process_file_data->source);
		ret = kExitCode_Error_ReadSource;
	}
	return ret;
}

static void finalize_plaintext_log_packet(bb_decoded_packet_t* decoded, const char* lineStart, size_t lineSize)
{
	if (lineSize > 1)
	{
		if (lineStart[lineSize - 1] == '\r')
		{
			--lineSize;
		}

		if (s_plaintext_prefix == kPlaintextPrefix_Auto && *lineStart != '[')
		{
			s_plaintext_prefix = kPlaintextPrefix_None;
		}

		if (s_plaintext_prefix != kPlaintextPrefix_None)
		{
			// const char* original = lineStart;
			const char* category = NULL;
			while (lineStart[0] == '[')
			{
				// step past [ ] blocks
				b32 valid = false;
				const char* start = lineStart;
				const char* end = start + 1;
				while (end && end < lineStart + lineSize)
				{
					if (*end == ']')
					{
						valid = true;
						break;
					}
					++end;
				}

				if (valid)
				{
					const char* keyword = va("%.*s", end - start - 1, start + 1);
					bb_log_level_e logLevel = bb_get_log_level_from_name(keyword);
					if (logLevel == kBBLogLevel_Count)
					{
						category = keyword;
					}
					else
					{
						decoded->packet.logText.level = logLevel;
					}

					size_t len = end - start + 1;
					lineStart += len;
					lineSize -= len;
				}
				else
				{
					break;
				}
			}

			if (category)
			{
				if (*category == ' ')
				{
					++lineStart;
					--lineSize;
				}

				if (s_plaintext_prefix == kPlaintextPrefix_Auto)
				{
					if ((*category >= 'a' && *category <= 'z') || (*category >= 'A' && *category <= 'Z'))
					{
						s_plaintext_prefix = kPlaintextPrefix_Brackets;
					}
					else
					{
						s_plaintext_prefix = kPlaintextPrefix_Unreal;
					}
				}

				if (s_plaintext_prefix == kPlaintextPrefix_Brackets)
				{
					decoded->packet.logText.categoryId = find_or_add_category_by_name(category);
				}
				else if (s_plaintext_prefix == kPlaintextPrefix_Unreal)
				{
					const char* start = lineStart;
					const char* end = start;
					while (end && end < lineStart + lineSize)
					{
						if (*end == ':')
						{
							category = va("%.*s", end - start, start);
							decoded->packet.logText.categoryId = find_or_add_category_by_name(category);
							size_t len = (end - start) + 2;
							lineStart += len;
							lineSize -= len;
							break;
						}
						else if ((*end >= 'a' && *end <= 'z') || (*end >= 'A' && *end <= 'Z'))
						{
							++end;
						}
						else
						{
							// not a "Category:" block
							break;
						}
					}

					if (decoded->packet.logText.categoryId > 0)
					{
						start = lineStart;
						end = start;
						while (end && end < lineStart + lineSize)
						{
							if (*end == ':')
							{
								category = va("%.*s", end - start, start);
								bb_log_level_e logLevel = bb_get_log_level_from_name(category);
								if (logLevel != kBBLogLevel_Count)
								{
									decoded->packet.logText.level = logLevel;
								}
								size_t len = (end - start) + 2;
								lineStart += len;
								lineSize -= len;
								break;
							}
							else if ((*end >= 'a' && *end <= 'z') || (*end >= 'A' && *end <= 'Z'))
							{
								++end;
							}
							else
							{
								// not a "Verbosity:" block
								break;
							}
						}
					}
				}
			}
			else
			{
				if (s_plaintext_prefix == kPlaintextPrefix_Auto)
				{
					s_plaintext_prefix = kPlaintextPrefix_None;
				}
			}
		}

		bb_strncpy(decoded->packet.logText.text, lineStart, lineSize);
	}
}

static int process_plaintext_file(process_file_data_t* process_file_data)
{
#if BB_USING(BB_COMPILER_MSVC)
	_locale_t utf8_locale = _create_locale(LC_ALL, ".utf8");
#endif
	static u8 recvBuffer[32768 + 1];
	static char mbsBuffer[32768 + 1];
	memset(recvBuffer, 0, sizeof(recvBuffer));
	memset(mbsBuffer, 0, sizeof(mbsBuffer));

	b32 bEncodingTested = false;
	b32 bUTF16 = false;
	b32 bByteSwap = false;

	bb_file_handle_t fp = bb_file_open_for_read(process_file_data->source);
	if (fp != BB_INVALID_FILE_HANDLE)
	{
		// recorded_session_queue_log_appinfo(session, filename);

		u32 recvCursor = 0;
		u32 decodeCursor = 0;
		u32 fileSize = 0;
		while (fp != BB_INVALID_FILE_HANDLE)
		{
			b32 done = false;
			u32 bytesToRead = sizeof(recvBuffer) - recvCursor - 2;
			if (bytesToRead % 2)
			{
				--bytesToRead;
			}
			u32 bytesRead = bb_file_read(fp, recvBuffer, bytesToRead);
			recvBuffer[bytesRead] = '\0';
			recvBuffer[bytesRead + 1] = '\0';

			if (!bEncodingTested)
			{
				bEncodingTested = true;
				if (bytesRead >= 2)
				{
					if ((recvBuffer[0] == 0xFF && recvBuffer[1] == 0xFE) || (recvBuffer[0] != 0 && recvBuffer[1] == 0))
					{
						bUTF16 = true;
					}
					else if ((recvBuffer[0] == 0xFE && recvBuffer[1] == 0xFF) || (recvBuffer[0] == 0 && recvBuffer[1] != 0))
					{
						bUTF16 = true;
						bByteSwap = true;
					}
				}
			}

			if (bByteSwap)
			{
				for (u32 index = 0; index < bytesRead; index += 2)
				{
					u8 tmp = recvBuffer[index];
					recvBuffer[index] = recvBuffer[index + 1];
					recvBuffer[index + 1] = tmp;
				}
			}

			if (bUTF16)
			{
				size_t mbsBufferSize = sizeof(mbsBuffer) - 1;
				mbsBuffer[0] = '\0';
#if BB_USING(BB_COMPILER_MSVC)
				size_t numCharsConverted;
				errno_t ret = _wcstombs_s_l(&numCharsConverted, mbsBuffer, mbsBufferSize, (const wchar_t*)recvBuffer, _TRUNCATE, utf8_locale);
#else
				int ret = wcstombs(mbsBuffer, (const wchar_t*)recvBuffer, mbsBufferSize);
				mbsBuffer[mbsBufferSize - 1] = '\0';
				size_t numCharsConverted = ret + 1;
#endif
				if (ret)
				{
					break;
				}

				mbsBuffer[numCharsConverted] = '\0';
				if (numCharsConverted > 0)
				{
					if (numCharsConverted - 1 >= sizeof(recvBuffer) + recvCursor)
					{
						break;
					}
					memcpy(recvBuffer + recvCursor, mbsBuffer, numCharsConverted);
					bytesRead = (u32)numCharsConverted - 1;
				}
			}
			else
			{
				memcpy(recvBuffer + recvCursor, recvBuffer, bytesRead);
			}

			if (bytesRead)
			{
				recvCursor += bytesRead;
			}
			else
			{
				u32 oldFileSize = fileSize;
				fileSize = bb_file_size(fp);
				if (fileSize < oldFileSize)
				{
					BB_LOG("Recorder::Read::Start", "restarting read from %s\n", process_file_data->source);
					bb_file_close(fp);
					fp = bb_file_open_for_read(process_file_data->source);
					recvCursor = 0;
					decodeCursor = 0;
					// bb_decoded_packet_t decoded = { BB_EMPTY_INITIALIZER };
					// decoded.type = kBBPacketType_Restart;
					// recorded_session_queue(session, &decoded);
					// recorded_session_queue_log_appinfo(session, filename);
					continue;
				}
				else
				{
					if (!bytesRead)
					{
						if (g_program == kProgram_bbtail)
						{
							if (g_inTailCatchup)
							{
								if (process_file_data->tail_catchup_func)
								{
									(*process_file_data->tail_catchup_func)(process_file_data);
								}
								g_inTailCatchup = false;
							}
						}
					}

					if (g_follow)
					{
						bb_sleep_ms(100);
					}
					else
					{
						done = true;
						bb_file_close(fp);
						fp = BB_INVALID_FILE_HANDLE;
					}
				}
			}

			while (!done)
			{
				const u32 krecvBufferSize = sizeof(recvBuffer);
				const u32 kHalfrecvBufferBytes = krecvBufferSize / 2;
				bb_decoded_packet_t decoded;
				u16 nDecodableBytes = (u16)(recvCursor - decodeCursor);
				if (nDecodableBytes < 1)
				{
					done = true;
					break;
				}

				const char* lineEnd = NULL;
				span_t cursor = { BB_EMPTY_INITIALIZER };
				cursor.start = (const char*)(recvBuffer + decodeCursor);
				cursor.end = (const char*)(recvBuffer + recvCursor);
				for (span_t line = tokenizeLine(&cursor); line.start; line = tokenizeLine(&cursor))
				{
					if (line.end && (*line.end == '\n' || !strncmp(line.end, "\r\n", 2)))
					{
						if (*line.end == '\r')
						{
							++line.end;
						}
						size_t lineLen = line.end - line.start;
						size_t maxLineLen = kBBSize_LogText - 1;
						while (lineLen > maxLineLen)
						{
							memset(&decoded, 0, sizeof(decoded));
							decoded.type = kBBPacketType_LogTextPartial;
							decoded.header.timestamp = bb_current_ticks();
							decoded.header.threadId = 0;
							decoded.header.fileId = 0;
							decoded.header.line = 0;
							decoded.packet.logText.level = kBBLogLevel_Log;
							finalize_plaintext_log_packet(&decoded, line.start, sizeof(decoded.packet.logText.text));
							bba_push(g_partialLogs, decoded);

							line.start += maxLineLen;
							lineLen -= maxLineLen;
						}

						memset(&decoded, 0, sizeof(decoded));
						decoded.type = kBBPacketType_LogText;
						decoded.header.timestamp = bb_current_ticks();
						decoded.header.threadId = 0;
						decoded.header.fileId = 0;
						decoded.header.line = 0;
						decoded.packet.logText.level = kBBLogLevel_Log;
						finalize_plaintext_log_packet(&decoded, line.start, lineLen + 1);
						if (process_file_data->packet_func)
						{
							(*process_file_data->packet_func)(&decoded, process_file_data);
						}

						lineEnd = line.end + 1;
					}
				}

				if (lineEnd)
				{
					u32 nConsumedBytes = (u32)(lineEnd - (const char*)(recvBuffer + decodeCursor));
					decodeCursor += nConsumedBytes;

					// TODO: rather lame to keep resetting the buffer - this should be a circular buffer
					if (decodeCursor >= kHalfrecvBufferBytes)
					{
						u16 nBytesRemaining = (u16)(recvCursor - decodeCursor);
						memmove(recvBuffer, recvBuffer + decodeCursor, nBytesRemaining);
						decodeCursor = 0;
						recvCursor = nBytesRemaining;
					}
				}
				else
				{
					done = true;
				}
			}
		}

		if (fp != BB_INVALID_FILE_HANDLE)
		{
			bb_file_close(fp);
		}
	}

#if BB_USING(BB_COMPILER_MSVC)
	_free_locale(utf8_locale);
#endif
	bba_free(g_categories);

	return process_file_data != 0;
}

int process_file(process_file_data_t* process_file_data)
{
	const char* ext = strrchr(process_file_data->source, '.');
	b32 plaintext = !ext || bb_stricmp(ext, ".bbox");
	int ret = (plaintext) ? process_plaintext_file(process_file_data) : process_bbox_file(process_file_data);

	for (u32 i = 0; i < g_queuedPackets.count; ++i)
	{
		reset_logPacket_t(g_queuedPackets.data + i);
	}
	bba_free(g_queuedPackets);

	return ret;
}

static void bbgrep_log_packet(bb_decoded_packet_t* decoded, process_file_data_t* process_file_data)
{
	vfilter_data_t* vfilter_data = process_file_data->userdata;
	vfilter_data->recordedLog.packet = *decoded;
	queue_packet(decoded, stdout, vfilter_data);
	vfilter_data->recordedLog.sessionLogIndex++;
}

static void bbgrep_file(const sb_t* path, const char* filter)
{
	vfilter_data_t vfilter_data = { BB_EMPTY_INITIALIZER };
	vfilter_data.view.vfilter = view_filter_parse("unnamed", filter);
	vfilter_data.view.config.filterActive = true;

	g_pathToPrint = sb_get(path);

	process_file_data_t process_file_data = { BB_EMPTY_INITIALIZER };

	process_file_data.source = g_pathToPrint;
	process_file_data.packet_func = &bbgrep_log_packet;
	process_file_data.userdata = &vfilter_data;

	process_file(&process_file_data);

	g_pathToPrint = NULL;
	vfilter_reset(&vfilter_data.view.vfilter);
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

typedef struct bboxtolog_userdata_s
{
	FILE* ofp;
} bboxtolog_userdata_t;

static void bboxtolog_tail_catchup(process_file_data_t* process_file_data)
{
	bboxtolog_userdata_t* bboxtolog_userdata = process_file_data->userdata;
	print_queued(bboxtolog_userdata->ofp);
}

static void bboxtolog_log_packet(bb_decoded_packet_t* decoded, process_file_data_t* process_file_data)
{
	bboxtolog_userdata_t* bboxtolog_userdata = process_file_data->userdata;
	queue_packet(decoded, bboxtolog_userdata->ofp, NULL);
}

typedef struct bboxtojson_userdata_s
{
	JSON_Value* value;
} bboxtojson_userdata_t;

static const char* get_packet_type_string(bb_packet_type_e type)
{
	switch (type)
	{
	case kBBPacketType_Invalid: return "kBBPacketType_Invalid";
	case kBBPacketType_AppInfo_v1: return "kBBPacketType_AppInfo_v1";
	case kBBPacketType_ThreadStart: return "kBBPacketType_ThreadStart";
	case kBBPacketType_ThreadName: return "kBBPacketType_ThreadName";
	case kBBPacketType_ThreadEnd: return "kBBPacketType_ThreadEnd";
	case kBBPacketType_FileId: return "kBBPacketType_FileId";
	case kBBPacketType_CategoryId: return "kBBPacketType_CategoryId";
	case kBBPacketType_FrameEnd: return "kBBPacketType_FrameEnd";
	case kBBPacketType_LogText_v1: return "kBBPacketType_LogText_v1";
	case kBBPacketType_UserToServer: return "kBBPacketType_UserToServer";
	case kBBPacketType_ConsoleCommand: return "kBBPacketType_ConsoleCommand";
	case kBBPacketType_UserToClient: return "kBBPacketType_UserToClient";
	case kBBPacketType_AppInfo_v2: return "kBBPacketType_AppInfo_v2";
	case kBBPacketType_AppInfo_v3: return "kBBPacketType_AppInfo_v3";
	case kBBPacketType_LogText_v2: return "kBBPacketType_LogText_v2";
	case kBBPacketType_LogText: return "kBBPacketType_LogText";
	case kBBPacketType_AppInfo_v4: return "kBBPacketType_AppInfo_v4";
	case kBBPacketType_LogTextPartial: return "kBBPacketType_LogTextPartial";
	case kBBPacketType_Restart: return "kBBPacketType_Restart";
	case kBBPacketType_StopRecording: return "kBBPacketType_StopRecording";
	case kBBPacketType_RecordingInfo: return "kBBPacketType_RecordingInfo";
	case kBBPacketType_ConsoleAutocompleteRequest: return "kBBPacketType_ConsoleAutocompleteRequest";
	case kBBPacketType_ConsoleAutocompleteResponseHeader: return "kBBPacketType_ConsoleAutocompleteResponseHeader";
	case kBBPacketType_ConsoleAutocompleteResponseEntry: return "kBBPacketType_ConsoleAutocompleteResponseEntry";
	case kBBPacketType_AppInfo_v5: return "kBBPacketType_AppInfo_v5";
	case kBBPacketType_FrameNumber: return "kBBPacketType_FrameNumber";
	default: return "unknown";
	}
}

static const char* get_bb_color_string(bb_color_t color)
{
	switch (color)
	{
	case kBBColor_Default: return "kBBColor_Default";
	case kBBColor_Evergreen_Black: return "kBBColor_Evergreen_Black";
	case kBBColor_Evergreen_Red: return "kBBColor_Evergreen_Red";
	case kBBColor_Evergreen_Green: return "kBBColor_Evergreen_Green";
	case kBBColor_Evergreen_Yellow: return "kBBColor_Evergreen_Yellow";
	case kBBColor_Evergreen_Blue: return "kBBColor_Evergreen_Blue";
	case kBBColor_Evergreen_Cyan: return "kBBColor_Evergreen_Cyan";
	case kBBColor_Evergreen_Pink: return "kBBColor_Evergreen_Pink";
	case kBBColor_Evergreen_White: return "kBBColor_Evergreen_White";
	case kBBColor_Evergreen_LightBlue: return "kBBColor_Evergreen_LightBlue";
	case kBBColor_Evergreen_Orange: return "kBBColor_Evergreen_Orange";
	case kBBColor_Evergreen_LightBlueAlt: return "kBBColor_Evergreen_LightBlueAlt";
	case kBBColor_Evergreen_OrangeAlt: return "kBBColor_Evergreen_OrangeAlt";
	case kBBColor_Evergreen_MediumBlue: return "kBBColor_Evergreen_MediumBlue";
	case kBBColor_Evergreen_Amber: return "kBBColor_Evergreen_Amber";
	case kBBColor_UE4_Black: return "kBBColor_UE4_Black";
	case kBBColor_UE4_DarkRed: return "kBBColor_UE4_DarkRed";
	case kBBColor_UE4_DarkGreen: return "kBBColor_UE4_DarkGreen";
	case kBBColor_UE4_DarkBlue: return "kBBColor_UE4_DarkBlue";
	case kBBColor_UE4_DarkYellow: return "kBBColor_UE4_DarkYellow";
	case kBBColor_UE4_DarkCyan: return "kBBColor_UE4_DarkCyan";
	case kBBColor_UE4_DarkPurple: return "kBBColor_UE4_DarkPurple";
	case kBBColor_UE4_DarkWhite: return "kBBColor_UE4_DarkWhite";
	case kBBColor_UE4_Red: return "kBBColor_UE4_Red";
	case kBBColor_UE4_Green: return "kBBColor_UE4_Green";
	case kBBColor_UE4_Blue: return "kBBColor_UE4_Blue";
	case kBBColor_UE4_Yellow: return "kBBColor_UE4_Yellow";
	case kBBColor_UE4_Cyan: return "kBBColor_UE4_Cyan";
	case kBBColor_UE4_Purple: return "kBBColor_UE4_Purple";
	case kBBColor_UE4_White: return "kBBColor_UE4_White";
	case kBBColor_Count: return "kBBColor_Count";
	default: return "unknown";
	}
}

static void json_object_set_header(JSON_Object* obj, bb_packet_header_t* header)
{
	json_object_set_string(obj, "timestamp", va("%llu", header->timestamp));
	json_object_set_string(obj, "threadId", va("%llu", header->threadId));
	json_object_set_number(obj, "fileId", header->fileId);
	json_object_set_number(obj, "line", header->line);
}

static void json_object_set_app_info(JSON_Object* obj, bb_packet_app_info_t* packet)
{
	json_object_set_string(obj, "initialTimestamp", va("%llu", packet->initialTimestamp));
	json_object_set_number(obj, "millisPerTick", packet->millisPerTick);
	json_object_set_string(obj, "applicationName", packet->applicationName);
	json_object_set_string(obj, "applicationGroup", packet->applicationGroup);
	json_object_set_number(obj, "initFlags", packet->initFlags);
	json_object_set_number(obj, "platform", packet->platform);
	json_object_set_string(obj, "microsecondsFromEpoch", va("%llu", packet->microsecondsFromEpoch));
}

static void json_object_set_text(JSON_Object* obj, bb_packet_text_t* packet)
{
	json_object_set_string(obj, "text", packet->text);
}

static void json_object_set_log_text(JSON_Object* obj, bb_packet_log_text_t* packet)
{
	json_object_set_number(obj, "categoryId", packet->categoryId);
	json_object_set_number(obj, "level", packet->level);
	json_object_set_number(obj, "pieInstance", packet->pieInstance);
	if (packet->colors.bg != kBBColor_Default)
	{
		json_object_set_string(obj, "bg", get_bb_color_string(packet->colors.bg));
	}
	if (packet->colors.fg != kBBColor_Default)
	{
		json_object_set_string(obj, "fg", get_bb_color_string(packet->colors.fg));
	}
	json_object_set_string(obj, "text", packet->text);
}

static void json_object_set_user(JSON_Object* obj, bb_packet_user_t* packet)
{
	json_object_set_number(obj, "len", packet->len);
	json_object_set_number(obj, "echo", packet->echo);
}

static void json_object_set_register_id(JSON_Object* obj, bb_packet_register_id_t* packet)
{
	json_object_set_number(obj, "id", packet->id);
	json_object_set_string(obj, "name", packet->name);
}

static void json_object_set_frame_end(JSON_Object* obj, bb_packet_frame_end_t* packet)
{
	json_object_set_number(obj, "milliseconds", packet->milliseconds);
}

static void json_object_set_recording_info(JSON_Object* obj, bb_packet_recording_info_t* packet)
{
	json_object_set_string(obj, "machineName", packet->machineName);
	json_object_set_string(obj, "recordingName", packet->recordingName);
}

static void json_object_set_console_autocomplete_request(JSON_Object* obj, bb_packet_console_autocomplete_request_t* packet)
{
	json_object_set_number(obj, "id", packet->id);
	json_object_set_string(obj, "text", packet->text);
}

static void json_object_set_console_autocomplete_response_header(JSON_Object* obj, bb_packet_console_autocomplete_response_header_t* packet)
{
	json_object_set_number(obj, "id", packet->id);
	json_object_set_number(obj, "total", packet->total);
	json_object_set_number(obj, "reuse", packet->reuse);
}

static void json_object_set_console_autocomplete_response_entry(JSON_Object* obj, bb_packet_console_autocomplete_response_entry_t* packet)
{
	json_object_set_number(obj, "id", packet->id);
	json_object_set_number(obj, "command", packet->command);
	json_object_set_number(obj, "flags", packet->flags);
	json_object_set_string(obj, "text", packet->text);
	json_object_set_string(obj, "description", packet->description);
}

static void json_object_set_frame_number(JSON_Object* obj, bb_packet_frame_number_t* header)
{
	json_object_set_string(obj, "frameNumber", va("%llu", header->frameNumber));
}

static void bboxtojson_packet(bb_decoded_packet_t* decoded, process_file_data_t* process_file_data)
{
	bboxtojson_userdata_t* bboxtojson_userdata = process_file_data->userdata;
	JSON_Array* arr = json_value_get_array(bboxtojson_userdata->value);
	if (!arr)
		return;

	JSON_Value* value = json_value_init_object();
	JSON_Object* obj = json_value_get_object(value);
	if (!value || !obj)
		return;

	json_object_set_string(obj, "type", get_packet_type_string(decoded->type));
	json_object_set_header(obj, &decoded->header);

	switch (decoded->type)
	{
	case kBBPacketType_Invalid: break;
	case kBBPacketType_AppInfo_v1: json_object_set_app_info(obj, &decoded->packet.appInfo); break;
	case kBBPacketType_ThreadStart: json_object_set_text(obj, &decoded->packet.text); break;
	case kBBPacketType_ThreadName: json_object_set_text(obj, &decoded->packet.text); break;
	case kBBPacketType_ThreadEnd: json_object_set_text(obj, &decoded->packet.text); break;
	case kBBPacketType_FileId: json_object_set_register_id(obj, &decoded->packet.registerId); break;
	case kBBPacketType_CategoryId: json_object_set_register_id(obj, &decoded->packet.registerId); break;
	case kBBPacketType_FrameEnd: json_object_set_frame_end(obj, &decoded->packet.frameEnd); break;
	case kBBPacketType_LogText_v1: json_object_set_log_text(obj, &decoded->packet.logText); break;
	case kBBPacketType_UserToServer: json_object_set_user(obj, &decoded->packet.user); break;
	case kBBPacketType_ConsoleCommand: json_object_set_text(obj, &decoded->packet.text); break;
	case kBBPacketType_UserToClient: json_object_set_user(obj, &decoded->packet.user); break;
	case kBBPacketType_AppInfo_v2: json_object_set_app_info(obj, &decoded->packet.appInfo); break;
	case kBBPacketType_AppInfo_v3: json_object_set_app_info(obj, &decoded->packet.appInfo); break;
	case kBBPacketType_LogText_v2: json_object_set_log_text(obj, &decoded->packet.logText); break;
	case kBBPacketType_LogText: json_object_set_log_text(obj, &decoded->packet.logText); break;
	case kBBPacketType_AppInfo_v4: json_object_set_app_info(obj, &decoded->packet.appInfo); break;
	case kBBPacketType_LogTextPartial: json_object_set_log_text(obj, &decoded->packet.logText); break;
	case kBBPacketType_Restart: break;
	case kBBPacketType_StopRecording: break;
	case kBBPacketType_RecordingInfo: json_object_set_recording_info(obj, &decoded->packet.recordingInfo); break;
	case kBBPacketType_ConsoleAutocompleteRequest: json_object_set_console_autocomplete_request(obj, &decoded->packet.consoleAutocompleteRequest); break;
	case kBBPacketType_ConsoleAutocompleteResponseHeader: json_object_set_console_autocomplete_response_header(obj, &decoded->packet.consoleAutocompleteResponseHeader); break;
	case kBBPacketType_ConsoleAutocompleteResponseEntry: json_object_set_console_autocomplete_response_entry(obj, &decoded->packet.consoleAutocompleteResponseEntry); break;
	case kBBPacketType_AppInfo_v5: json_object_set_app_info(obj, &decoded->packet.appInfo); break;
	case kBBPacketType_FrameNumber: json_object_set_frame_number(obj, &decoded->packet.frameNumber); break;
	default: break;
	}

	json_array_append_value(arr, value);
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
	else if (!bb_stricmp(g_exe, "bboxtojson"))
	{
		g_program = kProgram_bboxtojson;
	}
	else if (!bb_stricmp(g_exe, "bbstats"))
	{
		g_program = kProgram_bbstats;
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
			else if (!strcmp(arg, "-bboxtojson"))
			{
				g_program = kProgram_bboxtojson;
			}
			else if (!strcmp(arg, "-bbstats"))
			{
				g_program = kProgram_bbstats;
			}
			else if (!bb_strnicmp(arg, "-sql=", 5))
			{
				g_sqlCommand = arg + 5;
			}
			else if (!strcmp(arg, "-r"))
			{
				bRecursive = true;
			}
			else if (!strcmp(arg, "-H") || !strcmp(arg, "--with-filename"))
			{
				g_printFilename = true;
			}
			else if (!strcmp(arg, "-h") || !strcmp(arg, "--no-filename"))
			{
				g_printFilename = true;
			}
			else if (!bb_stricmp(arg, "--plaintext-prefix=unreal"))
			{
				s_plaintext_prefix = kPlaintextPrefix_Unreal;
			}
			else if (!bb_stricmp(arg, "--plaintext-prefix=brackets"))
			{
				s_plaintext_prefix = kPlaintextPrefix_Brackets;
			}
			else if (!bb_stricmp(arg, "--plaintext-prefix=none"))
			{
				s_plaintext_prefix = kPlaintextPrefix_None;
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

	if (g_program == kProgram_bbstats)
	{
		if (target)
		{
			bb_free(target);
		}
		return bbstats_main(argc, argv);
	}

	if (!source)
		return usage(); // TODO: read stdin

	if (target && g_program != kProgram_bboxtolog && g_program != kProgram_bbgrep && g_program != kProgram_bboxtojson)
	{
		bb_free(target);
		return usage();
	}

	if (g_follow && g_program == kProgram_bboxtolog)
	{
		if (target)
		{
			bb_free(target);
		}
		return usage();
	}

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

		DIR* d = opendir(target);
		b32 isdir = d != NULL;
		if (d)
		{
			closedir(d);
		}

		if (!isdir)
		{
			filename = path_get_filename(target);
			path_remove_filename(&dir);
		}
		if (sb_len(&dir) == 0)
		{
			sb_append(&dir, ".");
		}
		if (bRecursive || strchr(filename, '*') != NULL)
		{
			g_printFilename = true;
		}
		int ret = bbgrep(source, &dir, filename, bRecursive);
		sb_reset(&dir);
		if (target)
		{
			bb_free(target);
		}
		return ret;
	}

	size_t sourceLen = strlen(source);
	if (g_program == kProgram_bboxtolog || g_program == kProgram_bboxtojson)
	{
		if (sourceLen < 6 || bb_stricmp(source + sourceLen - 5, ".bbox"))
		{
			if (target)
			{
				bb_free(target);
			}
			return usage();
		}
	}
	else
	{
		if (sourceLen < 1)
		{
			if (target)
			{
				bb_free(target);
			}
			return usage();
		}
	}

	if (!target && (g_program == kProgram_bboxtolog))
	{
		target = bb_strdup(source);
		strcpy(target + sourceLen - 5, ".log");
	}
	else if (!target && (g_program == kProgram_bboxtojson))
	{
		target = bb_strdup(source);
		strcpy(target + sourceLen - 5, ".json");
	}

	// printf("source: %s\n", source);
	// if(target) {
	//	printf("target: %s\n", target);
	// }

	int ret = kExitCode_Success;

	if (g_program == kProgram_bboxtojson)
	{
		process_file_data_t process_file_data = { BB_EMPTY_INITIALIZER };
		bboxtojson_userdata_t userdata = { BB_EMPTY_INITIALIZER };

		process_file_data.source = source;
		process_file_data.packet_func = &bboxtojson_packet;
		process_file_data.userdata = &userdata;
		userdata.value = json_value_init_array();
		ret = process_file(&process_file_data);
		if (ret == kExitCode_Success)
		{
			if (json_serialize_to_file_pretty(userdata.value, target) != JSONSuccess)
			{
				if (target)
				{
					fprintf(stderr, "Could not write to %s\n", target);
				}
				ret = kExitCode_Error_WriteTarget;
			}
		}

		json_value_free(userdata.value);
	}
	else
	{
		process_file_data_t process_file_data = { BB_EMPTY_INITIALIZER };
		bboxtolog_userdata_t userdata = { BB_EMPTY_INITIALIZER };

		process_file_data.source = source;
		process_file_data.tail_catchup_func = &bboxtolog_tail_catchup;
		process_file_data.packet_func = &bboxtolog_log_packet;
		process_file_data.userdata = &userdata;
		userdata.ofp = (target) ? fopen(target, "wt") : stdout;

		if (userdata.ofp)
		{
			ret = process_file(&process_file_data);
		}
		else
		{
			if (target)
			{
				fprintf(stderr, "Could not write to %s\n", target);
			}
			ret = kExitCode_Error_WriteTarget;
		}
	}

	if (target)
	{
		bb_free(target);
	}

	return ret;
}

#if !defined(BB_NO_SQLITE)
static int sqlite_main_loop(int argc, char** argv)
{
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
}
#endif

int main(int argc, char** argv)
{
#if defined(_DEBUG)
	crt_leak_check_init();
	bb_tracked_malloc_enable(true);
	bba_set_logging(true, true);
#endif

#if defined(BB_NO_SQLITE)
	int ret = main_loop(argc, argv);
#else
	int ret = sqlite_main_loop(argc, argv);
#endif

	bba_free(g_partialLogs);

	BB_SHUTDOWN();

	return ret;
}
