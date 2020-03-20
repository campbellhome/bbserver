// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "recorded_session_thread.h"
#include "bb.h"
#include "bb_array.h"
#include "bb_file.h"
#include "bb_packet.h"
#include "bb_string.h"
#include "bb_thread.h"
#include "bb_time.h"
#include "file_utils.h"
#include "message_queue.h"
#include "recorded_session.h"
#include "span.h"
#include "tokenize.h"
#include "view.h"

#include "bb_wrap_stdio.h"

static void recorded_session_queue(recorded_session_t *session, bb_decoded_packet_t *decoded)
{
	session_message_queue_t *mq = session->incoming;
	bb_decoded_packet_t *message;
	while(mq->writeCursor - mq->readCursor == BB_ARRAYSIZE(mq->entries)) {
		if(!session->threadDesiredActive)
			return;
		bb_sleep_ms(0);
	}

	message = mq->entries + (mq->writeCursor % BB_ARRAYSIZE(mq->entries));
	memcpy(message, decoded, sizeof(*message));
	InterlockedIncrement64(&mq->writeCursor);
}

b32 recorded_session_consume(recorded_session_t *session, bb_decoded_packet_t *decoded)
{
	b32 result = false;
	session_message_queue_t *mq = session->incoming;
	u64 used = mq->writeCursor - mq->readCursor;
	if(used) {
		bb_decoded_packet_t *src = mq->entries + (mq->readCursor % BB_ARRAYSIZE(mq->entries));
		memcpy(decoded, src, sizeof(*decoded));
		InterlockedIncrement64(&mq->readCursor);
		result = true;
	}
	return result;
}

static void recorded_session_queue_log_appinfo(recorded_session_t *session, const char *filename)
{
	bb_decoded_packet_t decoded = { BB_EMPTY_INITIALIZER };
	decoded.type = kBBPacketType_AppInfo;
	decoded.header.timestamp = bb_current_ticks();
	decoded.header.threadId = 0;
	decoded.header.fileId = 0;
	decoded.header.line = 0;
	decoded.packet.appInfo.initialTimestamp = decoded.header.timestamp;
	decoded.packet.appInfo.millisPerTick = bb_millis_per_tick();
	decoded.packet.appInfo.initFlags = 0;
	decoded.packet.appInfo.platform = bb_platform();
	decoded.packet.appInfo.microsecondsFromEpoch = bb_current_time_microseconds_from_epoch();
	bb_strncpy(decoded.packet.appInfo.applicationName, filename, sizeof(decoded.packet.appInfo.applicationName));
	recorded_session_queue(session, &decoded);

	memset(&decoded, 0, sizeof(decoded));
	decoded.type = kBBPacketType_FileId;
	decoded.header.timestamp = bb_current_ticks();
	decoded.header.threadId = 0;
	decoded.header.fileId = 0;
	decoded.header.line = 0;
	decoded.packet.fileId.id = 0;
	bb_strncpy(decoded.packet.fileId.name, "Unknown", sizeof(decoded.packet.fileId.name));
	recorded_session_queue(session, &decoded);

	memset(&decoded, 0, sizeof(decoded));
	decoded.type = kBBPacketType_CategoryId;
	decoded.header.timestamp = bb_current_ticks();
	decoded.header.threadId = 0;
	decoded.header.fileId = 0;
	decoded.header.line = 0;
	decoded.packet.categoryId.id = 0;
	bb_strncpy(decoded.packet.categoryId.name, "Default", sizeof(decoded.packet.categoryId.name));
	recorded_session_queue(session, &decoded);

	memset(&decoded, 0, sizeof(decoded));
	decoded.type = kBBPacketType_ThreadName;
	decoded.header.timestamp = bb_current_ticks();
	decoded.header.threadId = 0;
	decoded.header.fileId = 0;
	decoded.header.line = 0;
	bb_strncpy(decoded.packet.threadName.text, "Unknown", sizeof(decoded.packet.threadName.text));
	recorded_session_queue(session, &decoded);
}

static void recorded_session_read_log(recorded_session_t *session, const char *filename)
{
	bb_file_handle_t fp = bb_file_open_for_read(session->path);
	if(fp != BB_INVALID_FILE_HANDLE) {
		recorded_session_queue_log_appinfo(session, filename);

		u32 recvCursor = 0;
		u32 decodeCursor = 0;
		u32 fileSize = 0;
		while(fp != BB_INVALID_FILE_HANDLE && session->threadDesiredActive && !session->failedToDeserialize) {
			b32 done = false;
			u32 bytesRead = bb_file_read(fp, session->recvBuffer + recvCursor, sizeof(session->recvBuffer) - recvCursor);
			if(bytesRead) {
				recvCursor += bytesRead;
			} else {
				u32 oldFileSize = fileSize;
				fileSize = bb_file_size(fp);
				if(fileSize < oldFileSize) {
					BB_LOG("Recorder::Read::Start", "restarting read from %s\n", session->path);
					bb_file_close(fp);
					fp = bb_file_open_for_read(session->path);
					recvCursor = 0;
					decodeCursor = 0;
					bb_decoded_packet_t decoded = { BB_EMPTY_INITIALIZER };
					decoded.type = kBBPacketType_Restart;
					recorded_session_queue(session, &decoded);
					recorded_session_queue_log_appinfo(session, filename);
					continue;
				} else {
					bb_sleep_ms(100);
				}
			}

			while(!done) {
				const u32 krecvBufferSize = sizeof(session->recvBuffer);
				const u32 kHalfrecvBufferBytes = krecvBufferSize / 2;
				bb_decoded_packet_t decoded;
				u16 nDecodableBytes = (u16)(recvCursor - decodeCursor);
				if(nDecodableBytes < 1) {
					done = true;
					break;
				}

				const char *lineEnd = NULL;
				span_t cursor = { BB_EMPTY_INITIALIZER };
				cursor.start = (const char *)(session->recvBuffer + decodeCursor);
				cursor.end = (const char *)(session->recvBuffer + recvCursor);
				for(span_t line = tokenizeLine(&cursor); line.start; line = tokenizeLine(&cursor)) {
					if(line.end && (*line.end == '\n' || !strncmp(line.end, "\r\n", 2))) {
						if(*line.end == '\r') {
							++line.end;
						}
						size_t lineLen = line.end - line.start;
						size_t maxLineLen = kBBSize_LogText - 1;
						while(lineLen > maxLineLen) {
							memset(&decoded, 0, sizeof(decoded));
							decoded.type = kBBPacketType_LogTextPartial;
							decoded.header.timestamp = bb_current_ticks();
							decoded.header.threadId = 0;
							decoded.header.fileId = 0;
							decoded.header.line = 0;
							decoded.packet.logText.level = kBBLogLevel_Log;
							bb_strncpy(decoded.packet.logText.text, line.start, sizeof(decoded.packet.logText.text));
							recorded_session_queue(session, &decoded);

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
						bb_strncpy(decoded.packet.logText.text, line.start, lineLen + 1);
						recorded_session_queue(session, &decoded);

						lineEnd = line.end + 1;
					}
				}

				if(lineEnd) {
					u32 nConsumedBytes = (u32)(lineEnd - (const char *)(session->recvBuffer + decodeCursor));
					decodeCursor += nConsumedBytes;

					// TODO: rather lame to keep resetting the buffer - this should be a circular buffer
					if(decodeCursor >= kHalfrecvBufferBytes) {
						u16 nBytesRemaining = (u16)(recvCursor - decodeCursor);
						memmove(session->recvBuffer, session->recvBuffer + decodeCursor, nBytesRemaining);
						decodeCursor = 0;
						recvCursor = nBytesRemaining;
					}
				} else {
					done = true;
				}
			}
		}

		if(fp != BB_INVALID_FILE_HANDLE) {
			bb_file_close(fp);
		}
	}
}

bb_thread_return_t recorded_session_read_thread(void *args)
{
	recorded_session_t *session = (recorded_session_t *)args;
	char threadName[1024];
	bb_file_handle_t fp;

	char *filename = strrchr(session->path, '\\');
	if(filename) {
		++filename;
	} else {
		filename = session->path;
	}

	if(bb_snprintf(threadName, sizeof(threadName), "read %s", filename) < 0) {
		threadName[sizeof(threadName) - 1] = '\0';
	}
	bbthread_set_name(threadName);
	BB_THREAD_START(threadName);
	BB_LOG("Recorder::Read::Start", "starting read from %s\n", session->path);

	const char *ext = strrchr(filename, '.');
	if(!ext || bb_stricmp(ext, ".bbox")) {
		recorded_session_read_log(session, filename);
	} else {
		fp = bb_file_open_for_read(session->path);
		if(fp != BB_INVALID_FILE_HANDLE) {
			u32 recvCursor = 0;
			u32 decodeCursor = 0;
			u32 fileSize = 0;
			while(fp != BB_INVALID_FILE_HANDLE && session->threadDesiredActive && !session->failedToDeserialize) {
				b32 done = false;
				u32 bytesRead = bb_file_read(fp, session->recvBuffer + recvCursor, sizeof(session->recvBuffer) - recvCursor);
				if(bytesRead) {
					recvCursor += bytesRead;
				} else {
					u32 oldFileSize = fileSize;
					fileSize = bb_file_size(fp);
					if(fileSize < oldFileSize) {
						BB_LOG("Recorder::Read::Start", "restarting read from %s\n", session->path);
						bb_file_close(fp);
						fp = bb_file_open_for_read(session->path);
						recvCursor = 0;
						decodeCursor = 0;
						bb_decoded_packet_t decoded = { BB_EMPTY_INITIALIZER };
						decoded.type = kBBPacketType_Restart;
						recorded_session_queue(session, &decoded);
						continue;
					} else {
						bb_sleep_ms(100);
					}
				}

				while(!done) {
					const u32 krecvBufferSize = sizeof(session->recvBuffer);
					const u32 kHalfrecvBufferBytes = krecvBufferSize / 2;
					bb_decoded_packet_t decoded;
					u16 nDecodableBytes = (u16)(recvCursor - decodeCursor);
					if(nDecodableBytes < 2) {
						done = true;
						break;
					}

					u8 *cursor = session->recvBuffer + decodeCursor;
					u16 nPacketBytes = (*cursor << 8) + (*(cursor + 1));
					if(!nPacketBytes) {
						BB_ERROR("Recorder::Read", "recieved 0-byte packet from %s\n", session->path);
						done = true;
						session->failedToDeserialize = true;
						break;
					}

					if(nPacketBytes > nDecodableBytes) {
						done = true;
						break;
					}

					if(bbpacket_deserialize(cursor + 2, nPacketBytes - 2, &decoded)) {
						recorded_session_queue(session, &decoded);
						if(session->logReads) {
							BB_LOG("Recorder::Read", "decoded packet type %d from %s\n", decoded.type, session->path);
						}
					} else {
						BB_ERROR("Recorder::Read", "failed to decode packet from %s\n", session->path);
						done = true;
						session->failedToDeserialize = true;
						break;
					}

					decodeCursor += nPacketBytes;

					// TODO: rather lame to keep resetting the buffer - this should be a circular buffer
					if(decodeCursor >= kHalfrecvBufferBytes) {
						u16 nBytesRemaining = (u16)(recvCursor - decodeCursor);
						memmove(session->recvBuffer, session->recvBuffer + decodeCursor, nBytesRemaining);
						decodeCursor = 0;
						recvCursor = nBytesRemaining;
					}
				}
			}

			if(fp != BB_INVALID_FILE_HANDLE) {
				bb_file_close(fp);
			}
		}
	}

	BB_LOG("Recorder::Read::Stop", "finished read from %s\n", session->path);
	BB_THREAD_END();
	session->threadActive = false;
	bb_thread_exit(0);
}
