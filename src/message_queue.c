// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "message_queue.h"
#include "bb_criticalsection.h"
#include "bb_thread.h"
#include "bb_time.h"

#include "bb_wrap_stdio.h"

typedef enum {
	kMessageQueue_ToUI,
	kMessageQueue_FirstDynamic,
	kMessageQueue_LastDynamic = kMessageQueue_FirstDynamic + 10,
	kMessageQueue_Count
} message_queue_type_e;

enum {
	kMessageQueue_Length = 128
};

typedef struct
{
	u64 refcount;
	u64 readCursor;
	u64 writeCursor;
	bb_critical_section cs;
	message_queue_message_t entries[kMessageQueue_Length];
} message_queue_t;

static message_queue_t s_mq[kMessageQueue_Count];
static bb_critical_section s_mq_reserve_cs;
static volatile b32 s_mq_shutting_down;

void mq_init(void)
{
	size_t i;
	for(i = 0; i < BB_ARRAYSIZE(s_mq); ++i) {
		bb_critical_section_init(&s_mq[i].cs);
	}
	bb_critical_section_init(&s_mq_reserve_cs);
}

void mq_pre_shutdown(void)
{
	s_mq_shutting_down = true;
}

void mq_shutdown(void)
{
	size_t i;
	for(i = 0; i < BB_ARRAYSIZE(s_mq); ++i) {
		bb_critical_section_shutdown(&s_mq[i].cs);
	}
	bb_critical_section_shutdown(&s_mq_reserve_cs);
}

b32 mq_vqueue(u32 queueId, u32 command, const char *fmt, va_list args)
{
	u64 used;
	message_queue_t *mq;
	b32 ret = false;
	char szBuffer[kMessageQueue_MessageSize];
	int len;
	if(queueId >= kMessageQueue_Count)
		return true;

	len = vsnprintf(szBuffer, sizeof(szBuffer), fmt, args);
	if(len == 0)
		return true;

	if(len < 0) {
		len = sizeof(szBuffer) - 1;
		szBuffer[len] = '\0';
	}

	mq = s_mq + queueId;

	bb_critical_section_lock(&mq->cs);

	used = mq->writeCursor - mq->readCursor;
	if(used < BB_ARRAYSIZE(mq->entries)) {
		message_queue_message_t *message = mq->entries + (mq->writeCursor % BB_ARRAYSIZE(mq->entries));
		message->command = command;
		memcpy(message->text, szBuffer, sizeof(message->text));
		++mq->writeCursor;
		ret = true;
	}

	bb_critical_section_unlock(&mq->cs);
	return ret;
}

b32 mq_consume(u32 queueId, message_queue_message_t *message)
{
	// #TODO: multi producer, single consumer shouldn't lock for read - just use InterlockedIncrement, InterlockedCompare
	b32 result = false;
	if(queueId < kMessageQueue_Count) {
		message_queue_t *mq = s_mq + queueId;
		u64 used = mq->writeCursor - mq->readCursor;
		if(used) {
			bb_critical_section_lock(&mq->cs);
			if(mq->writeCursor > mq->readCursor) {
				message_queue_message_t *src = mq->entries + (mq->readCursor % BB_ARRAYSIZE(mq->entries));
				memcpy(message, src, sizeof(*message));
				++mq->readCursor;
				result = true;
			}
			bb_critical_section_unlock(&mq->cs);
		}
	}
	return result;
}

const message_queue_message_t *mq_peek(u32 queueId)
{
	// #TODO: multi producer, single consumer shouldn't lock for read - just use InterlockedIncrement, InterlockedCompare
	message_queue_message_t *result = NULL;
	if(queueId < kMessageQueue_Count) {
		message_queue_t *mq = s_mq + queueId;
		u64 used = mq->writeCursor - mq->readCursor;
		if(used) {
			bb_critical_section_lock(&mq->cs);
			if(mq->writeCursor > mq->readCursor) {
				result = mq->entries + (mq->readCursor % BB_ARRAYSIZE(mq->entries));
			}
			bb_critical_section_unlock(&mq->cs);
		}
	}
	return result;
}

void mq_consume_peek_result(u32 queueId)
{
	// #TODO: multi producer, single consumer shouldn't lock for read - just use InterlockedIncrement, InterlockedCompare
	if(queueId < kMessageQueue_Count) {
		message_queue_t *mq = s_mq + queueId;
		u64 used = mq->writeCursor - mq->readCursor;
		if(used) {
			bb_critical_section_lock(&mq->cs);
			if(mq->writeCursor > mq->readCursor) {
				++mq->readCursor;
			}
			bb_critical_section_unlock(&mq->cs);
		}
	}
}

b32 mq_queue(u32 queueId, u32 command, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	b32 ret = mq_vqueue(queueId, command, fmt, args);
	va_end(args);
	return ret;
}

void to_ui(to_ui_command_e command, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	while(!mq_vqueue(kMessageQueue_ToUI, command, fmt, args) && !s_mq_shutting_down) {
		Sleep(10);
	}
	va_end(args);
}

b32 mq_consume_to_ui(message_queue_message_t *message)
{
	return mq_consume(kMessageQueue_ToUI, message);
}

u32 mq_invalid_id(void)
{
	return kMessageQueue_Count;
}

u32 mq_acquire(void)
{
	u32 ret = kMessageQueue_Count;
	bb_critical_section_lock(&s_mq_reserve_cs);
	for(u32 index = kMessageQueue_FirstDynamic; index <= kMessageQueue_LastDynamic; ++index) {
		if(!s_mq[index].refcount) {
			s_mq[index].refcount = 1;
			ret = index;
			break;
		}
	}
	bb_critical_section_unlock(&s_mq_reserve_cs);
	return ret;
}

u32 mq_addref(u32 id)
{
	u32 ret = kMessageQueue_Count;
	if(id >= kMessageQueue_FirstDynamic && id < kMessageQueue_Count) {
		bb_critical_section_lock(&s_mq_reserve_cs);
		if(s_mq[id].refcount) {
			++s_mq[id].refcount;
			ret = id;
		}
		bb_critical_section_unlock(&s_mq_reserve_cs);
	}
	return ret;
}

void mq_releaseref(u32 id)
{
	if(id >= kMessageQueue_FirstDynamic && id < kMessageQueue_Count) {
		bb_critical_section_lock(&s_mq_reserve_cs);
		if(s_mq[id].refcount) {
			--s_mq[id].refcount;
			if(!s_mq[id].refcount) {
				s_mq[id].readCursor = 0;
				s_mq[id].writeCursor = 0;
			}
		}
		bb_critical_section_unlock(&s_mq_reserve_cs);
	}
}
