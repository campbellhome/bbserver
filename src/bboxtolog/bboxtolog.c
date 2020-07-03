// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#if defined(_MSC_VER)
__pragma(warning(disable : 4710)); // warning C4710 : 'int printf(const char *const ,...)' : function not inlined
#endif

#include "bb.h"
#include "bbclient/bb_array.h"
#include "bbclient/bb_packet.h"
#include "bbclient/bb_string.h"
#include "bbclient/bb_time.h"
#include "sb.h"
#include "span.h"
#include "tokenize.h"

#include "bbclient/bb_wrap_malloc.h"
#include "bbclient/bb_wrap_stdio.h"
#include <stdlib.h>
#include <string.h>

BB_WARNING_DISABLE(5045);

typedef enum tag_program {
	kProgram_bboxtolog,
	kProgram_bbcat,
	kProgram_bbtail,
} program;

typedef enum tag_exitCode {
	kExitCode_Success,
	kExitCode_Error_Usage,
	kExitCode_Error_ReadSource,
	kExitCode_Error_WriteTarget,
	kExitCode_Error_Decode,
} exitCode;

typedef struct partial_logs_s {
	u32 count;
	u32 allocated;
	bb_decoded_packet_t *data;
} partial_logs_t;

typedef struct category_s {
	u32 id;
	char name[64];
} category_t;

typedef struct categories_s {
	category_t *data;
	u32 count;
	u32 allocated;
} categories_t;

typedef struct logPacket_s {
	s64 ms;
	sb_t lines;
	u32 categoryId;
	u8 pad[4];
} logPacket_t;

typedef struct logPackets_s {
	u32 count;
	u32 allocated;
	logPacket_t *data;
} logPackets_t;

static char *g_exe;
static program g_program;
static u8 g_recvBuffer[1 * 1024 * 1024];

static partial_logs_t g_partialLogs;
static bb_decoded_packet_t g_tailPackets[10];
static size_t g_nextTailPacketIndex;
static b32 g_inTailCatchup;
static b32 g_follow;
static u32 g_numLines;
static bb_log_level_e g_verbosity;

categories_t g_categories = { 0, 0, 0 };
double g_millisPerTick = 1.0;
u64 g_initialTimestamp = 0;
static logPackets_t g_queuedPackets;

static int usage(void)
{
	if(g_program == kProgram_bboxtolog) {
		fprintf(stderr, "Usage: %s filename.bbox <filename.log>\n", g_exe);
		fprintf(stderr, "If no output filename is specified, the target will be the source with .bbox\nextension replaced with .log\n");
	} else {
		fprintf(stderr, "Usage: %s filename.bbox\n", g_exe);
	}
	return kExitCode_Error_Usage;
}

static inline const char *get_category(u32 categoryId)
{
	const char *category = "";
	for(u32 i = 0; i < g_categories.count; ++i) {
		category_t *c = g_categories.data + i;
		if(c->id == categoryId) {
			category = c->name;
			break;
		}
	}
	return category;
}

static void print_lines(logPacket_t packet, FILE *ofp)
{
	const char *category = get_category(packet.categoryId);
	const char *lines = sb_get(&packet.lines);
	span_t linesCursor = span_from_string(lines);
	for(span_t line = tokenizeLine(&linesCursor); line.start; line = tokenizeLine(&linesCursor)) {
		sb_t text = sb_from_va("[%7lld] [%13s] %.*s\n", packet.ms, category, (int)(line.end - line.start), line.start);
		fputs(sb_get(&text), ofp);
#if BB_USING(BB_PLATFORM_WINDOWS)
		OutputDebugStringA(sb_get(&text));
#endif
		sb_reset(&text);
	}
}

static void reset_logPacket_t(logPacket_t *packet)
{
	sb_reset(&packet->lines);
}

static void queue_lines(sb_t lines, u32 categoryId, s64 ms, FILE *ofp)
{
	logPacket_t packet = { BB_EMPTY_INITIALIZER };
	packet.ms = ms;
	packet.lines = lines;
	packet.categoryId = categoryId;

	if(g_inTailCatchup) {
		if(g_queuedPackets.count >= g_numLines) {
			reset_logPacket_t(g_queuedPackets.data);
			bba_erase(g_queuedPackets, 0);
		}
		bba_push(g_queuedPackets, packet);
	} else {
		print_lines(packet, ofp);
		reset_logPacket_t(&packet);
	}
}

static s32 get_verbosity_sort(bb_log_level_e verbosity)
{
	switch(verbosity) {
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
static b32 test_verbosity(const bb_decoded_packet_t *decoded)
{
	bb_log_level_e verbosity = decoded->packet.logText.level;
	return (get_verbosity_sort(verbosity) >= get_verbosity_sort(g_verbosity));
}

static void queue_packet(const bb_decoded_packet_t *decoded, FILE *ofp)
{
	sb_t lines = { BB_EMPTY_INITIALIZER };
	for(u32 i = 0; i < g_partialLogs.count; ++i) {
		const bb_decoded_packet_t *partial = g_partialLogs.data + i;
		if(partial->header.threadId == decoded->header.threadId) {
			sb_append(&lines, partial->packet.logText.text);
		}
	}
	sb_append(&lines, decoded->packet.logText.text);

	s64 ms = (s64)((decoded->header.timestamp - g_initialTimestamp) * g_millisPerTick);
	if(test_verbosity(decoded)) {
		queue_lines(lines, decoded->packet.logText.categoryId, ms, ofp);
	} else {
		sb_reset(&lines);
	}

	for(u32 i = 0; i < g_partialLogs.count;) {
		const bb_decoded_packet_t *partial = g_partialLogs.data + i;
		if(partial->header.threadId == decoded->header.threadId) {
			bba_erase(g_partialLogs, i);
		} else {
			++i;
		}
	}
}

static void print_queued(FILE *ofp)
{
	for(u32 i = 0; i < g_queuedPackets.count; ++i) {
		logPacket_t *packet = g_queuedPackets.data + i;
		print_lines(*packet, ofp);
	}
}

int main(int argc, char **argv)
{
	g_exe = argv[0];
	char *sep = strrchr(g_exe, '\\');
	if(sep) {
		g_exe = sep + 1;
	}
	sep = strrchr(g_exe, '/');
	if(sep) {
		g_exe = sep + 1;
	}

	sep = strrchr(g_exe, '.');
	if(sep) {
		*sep = 0;
	}

	if(!bb_stricmp(g_exe, "bbcat")) {
		g_program = kProgram_bbcat;
	} else if(!bb_stricmp(g_exe, "bbtail")) {
		g_program = kProgram_bbtail;
	}

	const char *source = NULL;
	char *target = NULL;
	b32 bPastSwitches = false;
	for(int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if(!strcmp(arg, "--")) {
			bPastSwitches = true;
			continue;
		}

		if(!bPastSwitches && *arg == '-') {
			if(!strcmp(arg, "-f")) {
				g_follow = true;
			} else if(!strcmp(arg, "-n")) {
				if(i + 1 < argc) {
					g_numLines = atoi(argv[i + 1]);
					++i;
				} else {
					return usage();
				}
			} else if(!strcmp(arg, "-v") || !strcmp(arg, "-verbosity")) {
				if(i + 1 < argc) {
					++i;
					arg = argv[i];
					if(!bb_stricmp(arg, "log")) {
						g_verbosity = kBBLogLevel_Log;
					} else if(!bb_stricmp(arg, "warning")) {
						g_verbosity = kBBLogLevel_Warning;
					} else if(!bb_stricmp(arg, "error")) {
						g_verbosity = kBBLogLevel_Error;
					} else if(!bb_stricmp(arg, "display")) {
						g_verbosity = kBBLogLevel_Display;
					} else if(!bb_stricmp(arg, "veryverbose")) {
						g_verbosity = kBBLogLevel_VeryVerbose;
					} else if(!bb_stricmp(arg, "verbose")) {
						g_verbosity = kBBLogLevel_Verbose;
					} else if(!bb_stricmp(arg, "fatal")) {
						g_verbosity = kBBLogLevel_Fatal;
					} else {
						return usage();
					}
				} else {
					return usage();
				}
			} else if(!strcmp(arg, "-bbcat")) {
				g_program = kProgram_bbcat;
			} else if(!strcmp(arg, "-bbtail")) {
				g_program = kProgram_bbtail;
			} else {
				return usage();
			}
		} else {
			if(!source) {
				source = arg;
			} else if(!target) {
				target = bb_strdup(arg);
			} else {
				return usage();
			}
		}
	}

	if(!source)
		return usage(); // TODO: read stdin

	if(target && g_program != kProgram_bboxtolog)
		return usage();

	if(g_follow && g_program == kProgram_bboxtolog)
		return usage;

	if(g_program == kProgram_bbtail) {
		g_inTailCatchup = true;
	}

	if(g_numLines == 0) {
		g_numLines = 10;
	}

	size_t sourceLen = strlen(source);
	if(sourceLen < 6 || bb_stricmp(source + sourceLen - 5, ".bbox")) {
		return usage();
	}

	if(!target && (g_program == kProgram_bboxtolog)) {
		target = bb_strdup(source);
		strcpy(target + sourceLen - 5, ".log");
	}

	//printf("source: %s\n", source);
	//if(target) {
	//	printf("target: %s\n", target);
	//}

	int ret = kExitCode_Success;

	FILE *fp = fopen(source, "rb");
	if(fp) {
		FILE *ofp = (target) ? fopen(target, "wt") : stdout;
		if(ofp) {

			u32 recvCursor = 0;
			u32 decodeCursor = 0;
			b32 done = false;
			while(!done) {
				if(recvCursor < sizeof(g_recvBuffer)) {
					size_t bytesRead = fread(g_recvBuffer + recvCursor, 1, sizeof(g_recvBuffer) - recvCursor, fp);
					if(bytesRead) {
						recvCursor += (u32)bytesRead;
					}
				}
				const u32 krecvBufferSize = sizeof(g_recvBuffer);
				const u32 kHalfrecvBufferBytes = krecvBufferSize / 2;
				bb_decoded_packet_t decoded;
				u32 nDecodableBytes32 = recvCursor - decodeCursor;
				u16 nDecodableBytes = nDecodableBytes32 > 0xFFFF ? 0xFFFF : (u16)(nDecodableBytes32);
				u8 *cursor = g_recvBuffer + decodeCursor;
				u16 nPacketBytes = (nDecodableBytes >= 3) ? (*cursor << 8) + (*(cursor + 1)) : 0;
				if(nPacketBytes == 0 || nPacketBytes > nDecodableBytes) {
					if(g_program == kProgram_bbtail) {
						if(g_inTailCatchup) {
							print_queued(ofp);
							g_inTailCatchup = false;
						}
					}

					if(g_follow) {
						bb_sleep_ms(10);
						continue;
					} else {
						done = true;
						break;
					}
				}

				if(bbpacket_deserialize(cursor + 2, nPacketBytes - 2, &decoded)) {
					if(bbpacket_is_app_info_type(decoded.type)) {
						g_initialTimestamp = decoded.packet.appInfo.initialTimestamp;
						g_millisPerTick = decoded.packet.appInfo.millisPerTick;
					} else {
						BB_WARNING_PUSH(4061); // warning C4061: enumerator 'kBBPacketType_Invalid' in switch of enum 'bb_packet_type_e' is not explicitly handled by a case label
						switch(decoded.type) {
						case kBBPacketType_CategoryId: {
							category_t *c = bba_add(g_categories, 1);
							if(c) {
								const char *temp;
								const char *category = decoded.packet.categoryId.name;
								while((temp = strstr(category, "::")) != NULL) {
									if(*temp) {
										category = temp + 2;
									} else {
										break;
									}
								}
								c->id = decoded.packet.categoryId.id;
								bb_strncpy(c->name, category, sizeof(c->name));
							}
							break;
						}
						case kBBPacketType_LogTextPartial: {
							bba_push(g_partialLogs, decoded);
							break;
						}
						case kBBPacketType_LogText_v1:
						case kBBPacketType_LogText_v2:
						case kBBPacketType_LogText: {
							queue_packet(&decoded, ofp);
							break;
						}
						default:
							break;
						}
						BB_WARNING_POP;
					}
				} else {
					fprintf(stderr, "Failed to decode packet from %s\n", source);
					ret = kExitCode_Error_Decode;
					done = true;
				}

				decodeCursor += nPacketBytes;

				// TODO: rather lame to keep resetting the buffer - this should be a circular buffer
				if(decodeCursor >= kHalfrecvBufferBytes) {
					u32 nBytesRemaining = recvCursor - decodeCursor;
					memmove(g_recvBuffer, g_recvBuffer + decodeCursor, nBytesRemaining);
					decodeCursor = 0;
					recvCursor = nBytesRemaining;
				}
			}

			fclose(fp);
			fclose(ofp);
			bba_free(g_categories);
		} else {
			if(target) {
				fprintf(stderr, "Could not write to %s\n", target);
			}
			fclose(fp);
			ret = kExitCode_Error_WriteTarget;
		}
	} else {
		fprintf(stderr, "Could not read %s\n", source);
		ret = kExitCode_Error_ReadSource;
	}

	if(target) {
		free(target);
	}

	return ret;
}
