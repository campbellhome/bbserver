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

#include "bbclient/bb_wrap_malloc.h"
#include "bbclient/bb_wrap_stdio.h"
#include <string.h>

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

static char *g_exe;
static program g_program;
static u8 g_recvBuffer[1 * 1024 * 1024];

static bb_decoded_packet_t g_tailPackets[10];
static size_t g_nextTailPacketIndex;
static b32 g_inTailCatchup;

typedef struct category_s {
	u32 id;
	char name[64];
} category_t;

typedef struct categories_s {
	category_t *data;
	u32 count;
	u32 allocated;
} categories_t;

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

static inline void print_packet(const bb_decoded_packet_t *decoded, FILE *ofp, double millisPerTick, u64 initialTimestamp, const categories_t *categories)
{
	const char *category = "";
	int ms = (int)((decoded->header.timestamp - initialTimestamp) * millisPerTick);
	const char *text = decoded->packet.logText.text;
	size_t textLen = strlen(text);
	u32 i;
	for(i = 0; i < categories->count; ++i) {
		category_t *c = categories->data + i;
		if(c->id == decoded->packet.logText.categoryId) {
			category = c->name;
			break;
		}
	}
	fprintf(ofp, "[%7d] [%13s] %s%s",
	        ms, category, text,
	        textLen && text[textLen - 1] == '\n' ? "" : "\n");
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
		g_inTailCatchup = true;
	}

	if(g_program == kProgram_bboxtolog && (argc < 2 || argc > 3))
		return usage();

	if(g_program != kProgram_bboxtolog && argc != 2)
		return usage();

	const char *source = argv[1];

	size_t sourceLen = strlen(source);
	if(sourceLen < 6)
		return usage();

	if(bb_stricmp(source + sourceLen - 5, ".bbox")) {
		return usage();
	}

	char *target = (argc == 3) ? argv[2] : NULL;
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
			categories_t categories = { 0, 0, 0 };
			double millisPerTick = 1.0;
			u64 initialTimestamp = 0;

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
							size_t numPackets = g_nextTailPacketIndex > BB_ARRAYSIZE(g_tailPackets) ? BB_ARRAYSIZE(g_tailPackets) : g_nextTailPacketIndex;
							for(u32 i = 0; i < numPackets; ++i) {
								size_t packetIndex = g_nextTailPacketIndex - numPackets + i;
								print_packet(g_tailPackets + (packetIndex % BB_ARRAYSIZE(g_tailPackets)), ofp, millisPerTick, initialTimestamp, &categories);
							}
							g_inTailCatchup = false;
						}
						bb_sleep_ms(10);
						continue;
					} else {
						done = true;
						break;
					}
				}

				if(bbpacket_deserialize(cursor + 2, nPacketBytes - 2, &decoded)) {
					if(bbpacket_is_app_info_type(decoded.type)) {
						initialTimestamp = decoded.packet.appInfo.initialTimestamp;
						millisPerTick = decoded.packet.appInfo.millisPerTick;
					} else {
						switch(decoded.type) {
						case kBBPacketType_CategoryId: {
							category_t *c = bba_add(categories, 1);
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
						case kBBPacketType_LogText_v1:
						case kBBPacketType_LogText_v2:
						case kBBPacketType_LogText: {
							if(g_inTailCatchup) {
								size_t packetIndex = g_nextTailPacketIndex++ % BB_ARRAYSIZE(g_tailPackets);
								memcpy(g_tailPackets + packetIndex, &decoded, sizeof(decoded));
							} else {
								print_packet(&decoded, ofp, millisPerTick, initialTimestamp, &categories);
							}
							break;
						}
						default:
							break;
						}
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
			bba_free(categories);
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

	if(argc != 3 && target) {
		free(target);
	}

	return ret;
}
