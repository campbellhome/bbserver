// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_types.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
	kBBPacketType_Invalid,

	//
	// NOTE: ALWAYS ADD TO THE END OF THIS LIST TO AVOID BREAKING EXISTING PACKET STREAMS!!!
	//

	kBBPacketType_AppInfo_v1,   // Client --> Server
	kBBPacketType_ThreadStart,  // Client --> Server
	kBBPacketType_ThreadName,   // Client --> Server
	kBBPacketType_ThreadEnd,    // Client --> Server
	kBBPacketType_FileId,       // Client --> Server
	kBBPacketType_CategoryId,   // Client --> Server
	kBBPacketType_FrameEnd,     // Client --> Server
	kBBPacketType_LogText_v1,   // Client --> Server
	kBBPacketType_UserToServer, // Client --> Server

	kBBPacketType_ConsoleCommand, // Server --> Client
	kBBPacketType_UserToClient,   // Server --> Client

	kBBPacketType_AppInfo_v2, // Client --> Server
	kBBPacketType_AppInfo_v3, // Client --> Server
	kBBPacketType_LogText_v2, // Client --> Server
	kBBPacketType_LogText,    // Client --> Server
	kBBPacketType_AppInfo_v4, // Client --> Server

	kBBPacketType_LogTextPartial, // Client --> Server

	kBBPacketType_Restart,       // Not serialized or sent - used internally
	kBBPacketType_StopRecording, // Not serialized or sent - used internally

	kBBPacketType_RecordingInfo, // Server --> Client

	kBBPacketType_ConsoleAutocompleteRequest,        // Server --> Client
	kBBPacketType_ConsoleAutocompleteResponseHeader, // Client --> Server
	kBBPacketType_ConsoleAutocompleteResponseEntry,  // Client --> Server

	kBBPacketType_AppInfo_v5, // Client --> Server

	kBBPacketType_FrameNumber, // Client --> Server

	kBBPacketType_AppInfo_v6, // Client --> Server, same as kBBPacketType_AppInfo_v5 but indicates sorted verbosity enum

	kBBPacketType_AppInfo = kBBPacketType_AppInfo_v6

} bb_packet_type_e;

typedef struct bb_packet_header_s
{
	u64 timestamp;
	u64 threadId;
	u32 fileId;
	u32 line;
} bb_packet_header_t;

typedef struct bb_packet_app_info_s
{
	u64 initialTimestamp;
	double millisPerTick;
	char applicationName[kBBSize_ApplicationName];
	char applicationGroup[kBBSize_ApplicationName];
	u32 initFlags;
	u32 platform;
	u64 microsecondsFromEpoch;
} bb_packet_app_info_t;

typedef struct bb_packet_text_s
{
	char text[kBBSize_LogText];
} bb_packet_text_t;

typedef struct bb_packet_log_text_s
{
	u32 categoryId;
	u32 level;
	s32 pieInstance;
	bb_colors_t colors;
	char text[kBBSize_LogText];
} bb_packet_log_text_t;

typedef struct bb_packet_user_s
{
	u8 data[kBBSize_UserData];
	u16 len;
	b8 echo;
	u8 pad;
} bb_packet_user_t;

typedef struct bb_packet_register_id_s
{
	u32 id;
	char name[kBBSize_MaxPath];
} bb_packet_register_id_t;

typedef struct bb_packet_frame_end_s
{
	double milliseconds;
} bb_packet_frame_end_t;

typedef struct bb_packet_recording_info_s
{
	char machineName[kBBSize_MachineName];
	char recordingName[kBBSize_RecordingName];
} bb_packet_recording_info_t;

typedef struct bb_packet_console_autocomplete_request_s
{
	u32 id;
	char text[kBBSize_ConsoleAutocompleteText];
} bb_packet_console_autocomplete_request_t;

typedef struct bb_packet_console_autocomplete_response_header_s
{
	u32 id;
	u32 total;
	b32 reuse;
} bb_packet_console_autocomplete_response_header_t;

typedef struct bb_packet_console_autocomplete_response_entry_s
{
	u32 id;
	b32 command;
	u32 flags;
	char text[kBBSize_ConsoleAutocompleteText];
	char description[kBBSize_ConsoleAutocompleteDesc];
} bb_packet_console_autocomplete_response_entry_t;

typedef struct bb_packet_frame_number_s
{
	u64 frameNumber;
} bb_packet_frame_number_t;

typedef struct bb_decoded_packet_s
{
	bb_packet_type_e type;
	u8 pad[4];
	bb_packet_header_t header;
	union
	{
		bb_packet_app_info_t appInfo;
		bb_packet_text_t threadStart;
		bb_packet_text_t threadName;
		// bb_packet_text_t threadEnd; // unused
		bb_packet_register_id_t fileId;
		bb_packet_register_id_t categoryId;
		bb_packet_frame_end_t frameEnd;
		bb_packet_log_text_t logText;
		bb_packet_user_t userToServer;
		bb_packet_text_t consoleCommand;
		bb_packet_user_t userToClient;

		bb_packet_register_id_t registerId;
		bb_packet_text_t text;
		bb_packet_user_t user;

		bb_packet_recording_info_t recordingInfo;

		bb_packet_console_autocomplete_request_t consoleAutocompleteRequest;
		bb_packet_console_autocomplete_response_header_t consoleAutocompleteResponseHeader;
		bb_packet_console_autocomplete_response_entry_t consoleAutocompleteResponseEntry;

		bb_packet_frame_number_t frameNumber;
	} packet;
} bb_decoded_packet_t;

enum
{
	BB_MAX_PACKET_BUFFER_SIZE = sizeof(bb_decoded_packet_t)
};

BB_LINKAGE b32 bbpacket_deserialize(u8* buffer, u16 len, bb_decoded_packet_t* decoded);
BB_LINKAGE u16 bbpacket_serialize(bb_decoded_packet_t* source, u8* buffer, u16 len);
BB_LINKAGE b32 bbpacket_is_app_info_type(bb_packet_type_e type);
BB_LINKAGE b32 bbpacket_is_log_text_type(bb_packet_type_e type);

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
