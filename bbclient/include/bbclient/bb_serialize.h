// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum
{
	kBBSerialize_Ok,
	kBBSerialize_OutOfSpace,
} bbserialize_state_e;

typedef struct
{
	u8* pBuffer;
	u32 nBufferBytes;
	u32 nCursorBytes;
	bbserialize_state_e state;
	b32 reading;
} bb_serialize_t;

void bbserialize_init_read(bb_serialize_t* ser, void* buffer, u32 len);
void bbserialize_init_write(bb_serialize_t* ser, void* buffer, u32 len);
u32 bbserialize_get_cursor(bb_serialize_t* ser);
u32 bbserialize_get_remaining(bb_serialize_t* ser);
bbserialize_state_e bbserialize_get_state(bb_serialize_t* ser);
b32 bbserialize_is_reading(bb_serialize_t* ser);

b32 bbserialize_remaining_buffer(bb_serialize_t* ser, void* data, size_t dataSize, u16* dataLen);
b32 bbserialize_remaining_text_(bb_serialize_t* ser, char* data, size_t dataSize, size_t dataLen);
#define bbserialize_remaining_text(ser, data) bbserialize_remaining_text_(ser, data, sizeof(data), ser->reading ? 0 : strlen(data))

b32 bbserialize_buffer(bb_serialize_t* ser, void* data, u32 len);
b32 bbserialize_float(bb_serialize_t* ser, float* data);
b32 bbserialize_double(bb_serialize_t* ser, double* data);
b32 bbserialize_u64(bb_serialize_t* ser, u64* data);
b32 bbserialize_u32(bb_serialize_t* ser, u32* data);
b32 bbserialize_u16(bb_serialize_t* ser, u16* data);
b32 bbserialize_u8(bb_serialize_t* ser, u8* data);
b32 bbserialize_s64(bb_serialize_t* ser, s64* data);
b32 bbserialize_s32(bb_serialize_t* ser, s32* data);
b32 bbserialize_s16(bb_serialize_t* ser, s16* data);
b32 bbserialize_s8(bb_serialize_t* ser, s8* data);
b32 bbserialize_text_(bb_serialize_t* ser, char* data, size_t maxLen, u16* len);
#define bbserialize_text(ser, data, len) bbserialize_text_(ser, data, sizeof(data), len)

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
