// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"

#include "bbclient/bb_serialize.h"
#include <string.h> // memcpy

void bbserialize_init_read(bb_serialize_t* ser, void* buffer, u32 len)
{
	ser->pBuffer = (u8*)buffer;
	ser->nBufferBytes = len;
	ser->nCursorBytes = 0;
	ser->state = kBBSerialize_Ok;
	ser->reading = true;
}

void bbserialize_init_write(bb_serialize_t* ser, void* buffer, u32 len)
{
	ser->pBuffer = (u8*)buffer;
	ser->nBufferBytes = len;
	ser->nCursorBytes = 0;
	ser->state = kBBSerialize_Ok;
	ser->reading = false;
}

u32 bbserialize_get_cursor(bb_serialize_t* ser)
{
	return ser->nCursorBytes;
}

u32 bbserialize_get_remaining(bb_serialize_t* ser)
{
	return ser->nBufferBytes - ser->nCursorBytes;
}

bbserialize_state_e bbserialize_get_state(bb_serialize_t* ser)
{
	return ser->state;
}

b32 bbserialize_is_reading(bb_serialize_t* ser)
{
	return ser->reading != 0;
}

b32 bbserialize_remaining_buffer(bb_serialize_t* ser, void* data, size_t dataSize)
{
	size_t len = (ser->reading) ? bbserialize_get_remaining(ser) : dataSize;
	if (len > dataSize)
	{
		ser->state = kBBSerialize_OutOfSpace;
		return false;
	}
	else
	{
		bbserialize_buffer(ser, data, (u16)len);
		return ser->state == kBBSerialize_Ok;
	}
}

b32 bbserialize_remaining_text_(bb_serialize_t* ser, char* data, size_t dataSize, size_t dataLen)
{
	size_t len = (ser->reading) ? bbserialize_get_remaining(ser) : dataLen;
	if (len >= dataSize)
	{
		ser->state = kBBSerialize_OutOfSpace;
		return false;
	}
	else
	{
		bbserialize_buffer(ser, data, (u16)len);
		data[len] = '\0';
		return ser->state == kBBSerialize_Ok;
	}
}

b32 bbserialize_buffer(bb_serialize_t* ser, void* data, u32 len)
{
	b32 reading;
	void* pCursor;
	if (ser->state != kBBSerialize_Ok)
		return false;

	if (ser->nCursorBytes + len > ser->nBufferBytes)
	{
		ser->nCursorBytes += len;
		ser->state = kBBSerialize_OutOfSpace;
		return false;
	}

	reading = ser->reading != 0;
	pCursor = ser->pBuffer + ser->nCursorBytes;
	memcpy((reading) ? data : pCursor, (reading) ? pCursor : data, len);

	ser->nCursorBytes += len;

	return true;
}

b32 bbserialize_float(bb_serialize_t* ser, float* data)
{
	return bbserialize_buffer(ser, data, sizeof(*data));
}
b32 bbserialize_double(bb_serialize_t* ser, double* data)
{
	return bbserialize_buffer(ser, data, sizeof(*data));
}
b32 bbserialize_u64(bb_serialize_t* ser, u64* data)
{
	return bbserialize_buffer(ser, data, sizeof(*data));
}
b32 bbserialize_u32(bb_serialize_t* ser, u32* data)
{
	return bbserialize_buffer(ser, data, sizeof(*data));
}
b32 bbserialize_u16(bb_serialize_t* ser, u16* data)
{
	return bbserialize_buffer(ser, data, sizeof(*data));
}
b32 bbserialize_u8(bb_serialize_t* ser, u8* data)
{
	return bbserialize_buffer(ser, data, sizeof(*data));
}
b32 bbserialize_s64(bb_serialize_t* ser, s64* data)
{
	return bbserialize_buffer(ser, data, sizeof(*data));
}
b32 bbserialize_s32(bb_serialize_t* ser, s32* data)
{
	return bbserialize_buffer(ser, data, sizeof(*data));
}
b32 bbserialize_s16(bb_serialize_t* ser, s16* data)
{
	return bbserialize_buffer(ser, data, sizeof(*data));
}
b32 bbserialize_s8(bb_serialize_t* ser, s8* data)
{
	return bbserialize_buffer(ser, data, sizeof(*data));
}
b32 bbserialize_text_(bb_serialize_t* ser, char* data, size_t maxLen, u16* len)
{
	if (!ser->reading)
	{
		size_t realLen = strlen(data);
		if (realLen >= maxLen)
		{
			ser->state = kBBSerialize_OutOfSpace;
			return false;
		}
		*len = (u16)realLen;
	}
	bbserialize_u16(ser, len);
	if (*len >= maxLen)
	{
		ser->state = kBBSerialize_OutOfSpace;
		return false;
	}
	bbserialize_buffer(ser, data, *len);
	if (ser->reading)
	{
		data[*len] = '\0';
	}
	return ser->state == kBBSerialize_Ok;
}

#endif // #if BB_ENABLED
