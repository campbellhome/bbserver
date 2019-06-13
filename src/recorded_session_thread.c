// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "recorded_session_thread.h"
#include "bb.h"
#include "bb_thread.h"
#include "bbclient/bb_array.h"
#include "bbclient/bb_file.h"
#include "bbclient/bb_packet.h"
#include "bbclient/bb_string.h"
#include "bbclient/bb_time.h"
#include "message_queue.h"
#include "recorded_session.h"
#include "view.h"

#include "bbclient/bb_wrap_stdio.h"

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

	fp = bb_file_open_for_read(session->path);
	if(fp) {
		u32 recvCursor = 0;
		u32 decodeCursor = 0;
		u32 fileSize = 0;
		while(fp && session->threadDesiredActive && !session->failedToDeserialize) {
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

		if(fp) {
			bb_file_close(fp);
		}
	}

	BB_LOG("Recorder::Read::Stop", "finished read from %s\n", session->path);
	BB_THREAD_END();
	session->threadActive = false;
	bb_thread_exit(0);
}
