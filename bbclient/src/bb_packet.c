// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"

#include "bbclient/bb_assert.h"
#include "bbclient/bb_packet.h"
#include "bbclient/bb_serialize.h"
#include <string.h>

static b32 bbpacket_serialize_header(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	bbserialize_u64(ser, &decoded->header.timestamp);
	bbserialize_u64(ser, &decoded->header.threadId);
	bbserialize_u32(ser, &decoded->header.fileId);
	bbserialize_u32(ser, &decoded->header.line);
	return ser->state == kBBSerialize_Ok;
}

static b32 bbpacket_serialize_app_info_v1(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	bb_packet_app_info_t *packet = &decoded->packet.appInfo;
	if(ser->reading) {
		memset(packet, 0, sizeof(*packet));
	}
	bbserialize_u64(ser, &packet->initialTimestamp);
	bbserialize_double(ser, &packet->millisPerTick);
	return bbserialize_remaining_text(ser, packet->applicationName);
}

static b32 bbpacket_serialize_app_info_v2(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	bb_packet_app_info_t *packet = &decoded->packet.appInfo;
	if(ser->reading) {
		memset(packet, 0, sizeof(*packet));
	}
	bbserialize_u64(ser, &packet->initialTimestamp);
	bbserialize_double(ser, &packet->millisPerTick);
	bbserialize_u32(ser, &packet->initFlags);
	return bbserialize_remaining_text(ser, packet->applicationName);
}

static b32 bbpacket_serialize_app_info_v3(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	bb_packet_app_info_t *packet = &decoded->packet.appInfo;
	if(ser->reading) {
		memset(packet, 0, sizeof(*packet));
	}
	bbserialize_u64(ser, &packet->initialTimestamp);
	bbserialize_double(ser, &packet->millisPerTick);
	bbserialize_u32(ser, &packet->initFlags);
	bbserialize_u64(ser, &packet->microsecondsFromEpoch);
	return bbserialize_remaining_text(ser, packet->applicationName);
}

static b32 bbpacket_serialize_app_info(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	bb_packet_app_info_t *packet = &decoded->packet.appInfo;
	if(ser->reading) {
		memset(packet, 0, sizeof(*packet));
	}
	bbserialize_u64(ser, &packet->initialTimestamp);
	bbserialize_double(ser, &packet->millisPerTick);
	bbserialize_u32(ser, &packet->initFlags);
	bbserialize_u32(ser, &packet->platform);
	bbserialize_u64(ser, &packet->microsecondsFromEpoch);
	return bbserialize_remaining_text(ser, packet->applicationName);
}

static b32 bbpacket_serialize_user(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	bb_packet_user_t *user = &decoded->packet.user;
	return bbserialize_remaining_buffer(ser, user->data, user->len);
}

static b32 bbpacket_serialize_register_id(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	bb_packet_register_id_t *registerId = &decoded->packet.registerId;
	bbserialize_u32(ser, &registerId->id);
	return bbserialize_remaining_text(ser, registerId->name);
}

static b32 bbpacket_serialize_text(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	return bbserialize_remaining_text(ser, decoded->packet.text.text);
}

static b32 bbpacket_serialize_log_text_v1(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	if(ser->reading) {
		decoded->packet.logText.colors.fg = kBBColor_Default;
		decoded->packet.logText.colors.bg = kBBColor_Default;
		decoded->packet.logText.pieInstance = 0;
	}
	bbserialize_u32(ser, &decoded->packet.logText.categoryId);
	bbserialize_u32(ser, &decoded->packet.logText.level);
	return bbserialize_remaining_text(ser, decoded->packet.logText.text);
}

static b32 bbpacket_serialize_log_text_v2(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	if(ser->reading) {
		decoded->packet.logText.pieInstance = 0;
	}
	bbserialize_u32(ser, &decoded->packet.logText.categoryId);
	bbserialize_u32(ser, &decoded->packet.logText.level);
	bbserialize_s32(ser, (s32 *)&decoded->packet.logText.colors.fg);
	bbserialize_s32(ser, (s32 *)&decoded->packet.logText.colors.bg);
	return bbserialize_remaining_text(ser, decoded->packet.logText.text);
}

static b32 bbpacket_serialize_log_text(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	bbserialize_u32(ser, &decoded->packet.logText.categoryId);
	bbserialize_u32(ser, &decoded->packet.logText.level);
	bbserialize_s32(ser, &decoded->packet.logText.pieInstance);
	bbserialize_s32(ser, (s32 *)&decoded->packet.logText.colors.fg);
	bbserialize_s32(ser, (s32 *)&decoded->packet.logText.colors.bg);
	return bbserialize_remaining_text(ser, decoded->packet.logText.text);
}

static b32 bbpacket_serialize_frameend(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	return bbserialize_double(ser, &decoded->packet.frameEnd.milliseconds);
}

static b32 bbpacket_serialize_recording_info(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	u16 len = 0;
	bbserialize_text(ser, decoded->packet.recordingInfo.machineName, &len);
	return bbserialize_text(ser, decoded->packet.recordingInfo.recordingName, &len);
}

static b32 bbpacket_serialize_console_autocomplete_request(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	bb_packet_console_autocomplete_request_t *consoleAutocompleteRequest = &decoded->packet.consoleAutocompleteRequest;
	bbserialize_u32(ser, &consoleAutocompleteRequest->id);
	return bbserialize_remaining_text(ser, consoleAutocompleteRequest->text);
}

static b32 bbpacket_serialize_console_autocomplete_response_header(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	bb_packet_console_autocomplete_response_header_t *consoleAutocompleteResponseHeader = &decoded->packet.consoleAutocompleteResponseHeader;
	bbserialize_u32(ser, &consoleAutocompleteResponseHeader->id);
	return bbserialize_u32(ser, &consoleAutocompleteResponseHeader->total);
}

static b32 bbpacket_serialize_console_autocomplete_response_entry(bb_serialize_t *ser, bb_decoded_packet_t *decoded)
{
	bb_packet_console_autocomplete_response_entry_t *consoleAutocompleteResponseEntry = &decoded->packet.consoleAutocompleteResponseEntry;
	bbserialize_u32(ser, &consoleAutocompleteResponseEntry->id);
	bbserialize_s32(ser, &consoleAutocompleteResponseEntry->command);
	bbserialize_u32(ser, &consoleAutocompleteResponseEntry->flags);
	u16 len = 0;
	bbserialize_text(ser, consoleAutocompleteResponseEntry->text, &len);
	return bbserialize_text(ser, consoleAutocompleteResponseEntry->description, &len);
}

b32 bbpacket_deserialize(u8 *buffer, u16 len, bb_decoded_packet_t *decoded)
{
	u8 type;
	bb_serialize_t ser;
	bbserialize_init_read(&ser, buffer, len);
	type = kBBPacketType_Invalid;
	bbserialize_u8(&ser, &type);
	bbpacket_serialize_header(&ser, decoded);
	decoded->type = (bb_packet_type_e)type;
	switch(decoded->type) {
	case kBBPacketType_AppInfo_v1:
		return bbpacket_serialize_app_info_v1(&ser, decoded);

	case kBBPacketType_AppInfo_v2:
		return bbpacket_serialize_app_info_v2(&ser, decoded);

	case kBBPacketType_AppInfo_v3:
		return bbpacket_serialize_app_info_v3(&ser, decoded);

	case kBBPacketType_AppInfo:
		return bbpacket_serialize_app_info(&ser, decoded);

	case kBBPacketType_ThreadStart:
	case kBBPacketType_ThreadName:
	case kBBPacketType_ConsoleCommand:
		return bbpacket_serialize_text(&ser, decoded);

	case kBBPacketType_LogText_v1:
		return bbpacket_serialize_log_text_v1(&ser, decoded);

	case kBBPacketType_LogText_v2:
		return bbpacket_serialize_log_text_v2(&ser, decoded);

	case kBBPacketType_LogTextPartial:
	case kBBPacketType_LogText:
		return bbpacket_serialize_log_text(&ser, decoded);

	case kBBPacketType_FileId:
	case kBBPacketType_CategoryId:
		return bbpacket_serialize_register_id(&ser, decoded);

	case kBBPacketType_ThreadEnd:
		return ser.state == kBBSerialize_Ok;

	case kBBPacketType_FrameEnd:
		return bbpacket_serialize_frameend(&ser, decoded);

	case kBBPacketType_UserToServer:
	case kBBPacketType_UserToClient:
		return bbpacket_serialize_user(&ser, decoded);

	case kBBPacketType_RecordingInfo:
		return bbpacket_serialize_recording_info(&ser, decoded);

	case kBBPacketType_ConsoleAutocompleteRequest:
		return bbpacket_serialize_console_autocomplete_request(&ser, decoded);

	case kBBPacketType_ConsoleAutocompleteResponseHeader:
		return bbpacket_serialize_console_autocomplete_response_header(&ser, decoded);

	case kBBPacketType_ConsoleAutocompleteResponseEntry:
		return bbpacket_serialize_console_autocomplete_response_entry(&ser, decoded);

	case kBBPacketType_Invalid:
	case kBBPacketType_Restart:
	case kBBPacketType_StopRecording:
	default:
		return false;
	}
}

u16 bbpacket_serialize(bb_decoded_packet_t *source, u8 *buffer, u16 len)
{
	u8 type;
	bb_serialize_t ser;
	bbserialize_init_write(&ser, buffer, len);
	type = (u8)source->type;
	bbserialize_u8(&ser, &type);
	bbpacket_serialize_header(&ser, source);
	switch(source->type) {
	case kBBPacketType_AppInfo_v1:
		bbpacket_serialize_app_info_v1(&ser, source);
		break;

	case kBBPacketType_AppInfo_v2:
		bbpacket_serialize_app_info_v2(&ser, source);
		break;

	case kBBPacketType_AppInfo_v3:
		bbpacket_serialize_app_info_v3(&ser, source);
		break;

	case kBBPacketType_AppInfo:
		bbpacket_serialize_app_info(&ser, source);
		break;

	case kBBPacketType_ThreadStart:
	case kBBPacketType_ThreadName:
	case kBBPacketType_ConsoleCommand:
		bbpacket_serialize_text(&ser, source);
		break;

	case kBBPacketType_LogText_v1:
		bbpacket_serialize_log_text_v1(&ser, source);
		break;

	case kBBPacketType_LogText_v2:
		bbpacket_serialize_log_text_v2(&ser, source);
		break;

	case kBBPacketType_LogTextPartial:
	case kBBPacketType_LogText:
		bbpacket_serialize_log_text(&ser, source);
		break;

	case kBBPacketType_FileId:
	case kBBPacketType_CategoryId:
		bbpacket_serialize_register_id(&ser, source);
		break;

	case kBBPacketType_ThreadEnd:
		break;

	case kBBPacketType_FrameEnd:
		bbpacket_serialize_frameend(&ser, source);
		break;

	case kBBPacketType_UserToServer:
	case kBBPacketType_UserToClient:
		bbpacket_serialize_user(&ser, source);
		break;

	case kBBPacketType_RecordingInfo:
		bbpacket_serialize_recording_info(&ser, source);
		break;

	case kBBPacketType_ConsoleAutocompleteRequest:
		bbpacket_serialize_console_autocomplete_request(&ser, source);
		break;

	case kBBPacketType_ConsoleAutocompleteResponseHeader:
		bbpacket_serialize_console_autocomplete_response_header(&ser, source);
		break;

	case kBBPacketType_ConsoleAutocompleteResponseEntry:
		bbpacket_serialize_console_autocomplete_response_entry(&ser, source);
		break;

	case kBBPacketType_Invalid:
	case kBBPacketType_Restart:
	case kBBPacketType_StopRecording:
	default:
		BB_ASSERT(false);
		return 0;
	}

	if(ser.state == kBBSerialize_Ok) {
		return (u16)ser.nCursorBytes; // #TODO: bb_serialize_t len should be u16 (or everything else should be u32)
	} else {
		return 0;
	}
}

b32 bbpacket_is_app_info_type(bb_packet_type_e type)
{
	return type == kBBPacketType_AppInfo_v1 ||
	       type == kBBPacketType_AppInfo_v2 ||
	       type == kBBPacketType_AppInfo_v3 ||
	       type == kBBPacketType_AppInfo;
}

b32 bbpacket_is_log_text_type(bb_packet_type_e type)
{
	return type == kBBPacketType_LogText_v1 ||
	       type == kBBPacketType_LogText_v2 ||
	       type == kBBPacketType_LogText;
}

#endif // #if BB_ENABLED
