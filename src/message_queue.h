// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "config.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
	kToUI_DiscoveryStatus,
	kToUI_AddExistingFile,
	kToUI_AddInvalidExistingFile,
	kToUI_RecordingStart,
	kToUI_RecordingStop,
} to_ui_command_e;

enum {
	kMessageQueue_MessageSize = 2040
};

AUTOSTRUCT typedef struct message_queue_message_s {
	u32 command;
	u32 userData;
	char text[kMessageQueue_MessageSize];
} message_queue_message_t;

AUTOSTRUCT typedef struct message_queue_messages_s {
	u32 count;
	u32 allocated;
	message_queue_message_t *data;
} message_queue_messages_t;

void mq_init(void);
void mq_pre_shutdown(void);
void mq_shutdown(void);

//
// dynamic acquisition of message queues
//
u32 mq_invalid_id(void);
u32 mq_acquire(void);
u32 mq_addref(u32 id);
void mq_releaseref(u32 id);

//
// queue produce/consume for any (dynamic) queue
//
b32 mq_vqueue(u32 queueId, u32 command, const char *fmt, va_list args);
b32 mq_queue(u32 queueId, u32 command, const char *fmt, ...);
b32 mq_queue_userData(u32 queueId, u32 command, u32 userData, const char* text, u32 textLen);
b32 mq_consume(u32 queueId, message_queue_message_t *message);
const message_queue_message_t *mq_peek(u32 queueId);
void mq_consume_peek_result(u32 queueId);

//
// convenience functions for UI queue
//
void to_ui(to_ui_command_e command, const char *fmt, ...);
b32 mq_consume_to_ui(message_queue_message_t *message);

#if defined(__cplusplus)
}
#endif
