// Copyright (c) 2012-2024 Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#if defined(_MSC_VER)
__pragma(warning(disable : 4710)) // warning C4710 : 'int printf(const char *const ,...)' : function not inlined
#endif

#include "bb.h"

#include "bbclient/bb_array.h"
#include "bbclient/bb_assert.h"
#include "bbclient/bb_connection.h"
#include "bbclient/bb_criticalsection.h"
#include "bbclient/bb_discovery_client.h"
#include "bbclient/bb_discovery_shared.h"
#include "bbclient/bb_file.h"
#include "bbclient/bb_log.h"
#include "bbclient/bb_malloc.h"
#include "bbclient/bb_packet.h"
#include "bbclient/bb_string.h"
#include "bbclient/bb_time.h"
#include "bbclient/bb_wrap_stdio.h"
#include <stdlib.h>
#include <wchar.h>

#if BB_USING(BB_COMPILER_MSVC)
#include "bbclient/bb_wrap_windows.h"
#define bb_thread_local __declspec(thread)
u64 bb_get_current_thread_id(void)
{
	return GetCurrentThreadId();
}
#else // #if BB_USING(BB_COMPILER_MSVC)
#define bb_thread_local __thread
#if BB_USING(BB_PLATFORM_LINUX)
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#endif // #if BB_USING(BB_PLATFORM_LINUX)
u64 bb_get_current_thread_id(void)
{
#if BB_USING(BB_PLATFORM_LINUX)
	return syscall(SYS_gettid);
#else  // #if BB_USING(BB_PLATFORM_LINUX)
	return (u64)pthread_self();
#endif // #else // #if BB_USING(BB_PLATFORM_LINUX)
}
#endif // #else // #if BB_USING(BB_COMPILER_MSVC)

typedef struct bb_id_s
{
	bb_packet_header_t header;
	bb_packet_type_e packetType;
	u32 id;
	char* name;
} bb_id_t;
typedef struct bb_ids_s
{
	u32 count;
	u32 allocated;
	bb_id_t* data;
	u32 lastId;
	u8 pad[4];
} bb_ids_t;

static bb_ids_t s_bb_categoryIds;
static bb_ids_t s_bb_pathIds;
static bb_ids_t s_bb_threadIds;

static bb_connection_t s_con;
static bb_critical_section s_id_cs;
static bb_file_handle_t s_fp = BB_INVALID_FILE_HANDLE;
static u64 s_lastFileFlushTime;
static char s_deviceCode[kBBSize_ApplicationName];
static char s_sourceApplicationName[kBBSize_ApplicationName];
static char s_applicationName[kBBSize_ApplicationName];
static char s_applicationGroup[kBBSize_ApplicationName];
extern u32 g_bb_initFlags; // we don't have a good header for this, so we need to avoid a warning from -Wmissing-variable-declarations
u32 g_bb_initFlags;
static u32 s_sourceIp;
static u32 s_serverIp;
static u16 s_serverPort;
static b32 s_bCallbackSentAppInfo;
static b32 s_bFileSentAppInfo;
static b32 s_bDisableStoredThreadIds;
static bb_write_callback s_bb_write_callback;
static void* s_bb_write_callback_context;
static bb_flush_callback s_bb_flush_callback;
static void* s_bb_flush_callback_context;
static bb_send_callback s_bb_send_callback;
static void* s_bb_send_callback_context;
static bb_incoming_packet_handler s_bb_incoming_packet_handler;
static void* s_bb_incoming_packet_context;

enum bbInitialBufferState_e
{
	kBBInitialBuffer_Unset,
	kBBInitialBuffer_Set,
	kBBInitialBuffer_Done,
};
typedef struct bbInitialBuffer_s
{
	bb_critical_section cs;
	u64 state;
	void* data;
	u32 size;
	u32 used;
	u32 start;
	u32 pad;
} bbInitialBuffer_t;
static bbInitialBuffer_t s_initial_buffer;

bb_decoded_packet_t s_initialAppInfo;

typedef struct bbtraceBuffer_s
{
	char packetBuffer[16 * 1024];
#if BB_COMPILE_WIDECHAR
	bb_wchar_t wideBuffer[16 * 1024];
#endif
} bbtraceBuffer_t;

static bb_thread_local bbtraceBuffer_t* s_bb_trace_packet_buffer;
static bb_thread_local bb_colors_t s_bb_colors;
void bb_set_color(bb_color_t fg, bb_color_t bg)
{
	bb_trace_partial_end();
	s_bb_colors.fg = fg;
	s_bb_colors.bg = bg;
}

enum
{
	kBBFile_FlushIntervalMillis = 500,
};

#if BB_COMPILE_WIDECHAR

#if defined(BB_USER_WCSTOMBCS) && BB_USER_WCSTOMBCS

extern const char* bb_wcstombcs(const bb_wchar_t* wstr);
extern const char* bb_wcstombcs_inline(const bb_wchar_t* wstr, char* buffer, size_t bufferSize, size_t* numCharsConverted);
extern int bb_vswprintf(bb_wchar_t* dest, int count, const bb_wchar_t* fmt, va_list args);
extern void bb_init_locale(void);
extern void bb_shutdown_locale(void);

#else // #if defined(BB_USER_WCSTOMBCS) && BB_USER_WCSTOMBCS

#if BB_USING(BB_COMPILER_MSVC)
#include <locale.h>
static _locale_t s_utf8_locale;
#endif
enum
{
	kBB_WCSToMBCS_NumSlots = 4
};
typedef struct
{
	size_t next;
	char buffer[kBB_WCSToMBCS_NumSlots][2048];
} bb_wcstombcs_data_t;
static bb_thread_local bb_wcstombcs_data_t s_bb_wcstombcs_data;

static BB_INLINE const char* bb_wcstombcs(const bb_wchar_t* wstr)
{
	char* buffer = s_bb_wcstombcs_data.buffer[s_bb_wcstombcs_data.next++ % kBB_WCSToMBCS_NumSlots];
	size_t bufferSize = sizeof(s_bb_wcstombcs_data.buffer[0]);
	buffer[0] = '\0';
	if (wstr)
	{
#if BB_USING(BB_COMPILER_MSVC)
		size_t numCharsConverted;
		_wcstombs_s_l(&numCharsConverted, buffer, bufferSize, wstr, _TRUNCATE, s_utf8_locale);
#else
		wcstombs(buffer, wstr, bufferSize);
		buffer[bufferSize - 1] = '\0';
#endif
	}
	return buffer;
}

static BB_INLINE const char* bb_wcstombcs_inline(const bb_wchar_t* wstr, char* buffer, size_t bufferSize, size_t* numCharsConverted)
{
	buffer[0] = '\0';
#if BB_USING(BB_COMPILER_MSVC)
	_wcstombs_s_l(numCharsConverted, buffer, bufferSize, wstr, _TRUNCATE, s_utf8_locale);
#else
	*numCharsConverted = 1 + wcstombs(buffer, wstr, bufferSize);
	if (*numCharsConverted)
	{
		buffer[*numCharsConverted - 1] = '\0';
	}
#endif
	return buffer;
}

static BB_INLINE void bb_init_locale(void)
{
#if BB_USING(BB_COMPILER_MSVC)
	if (!s_utf8_locale)
	{
		s_utf8_locale = _create_locale(LC_ALL, ".utf8");
	}
#endif
}

static BB_INLINE void bb_shutdown_locale(void)
{
#if BB_USING(BB_COMPILER_MSVC)
	if (s_utf8_locale)
	{
		_free_locale(s_utf8_locale);
		s_utf8_locale = NULL;
	}
#endif
}

#endif // #else // #if defined(BB_USER_WCSTOMBCS) && BB_USER_WCSTOMBCS

#else  // #if BB_COMPILE_WIDECHAR
static BB_INLINE void bb_init_locale(void)
{
}

static BB_INLINE void bb_shutdown_locale(void)
{
}
#endif // #else // #if BB_COMPILE_WIDECHAR

static BB_INLINE void bb_fill_header(bb_decoded_packet_t* decoded, bb_packet_type_e packetType, u32 pathId, u32 line)
{
	decoded->type = packetType;
	decoded->header.timestamp = bb_current_ticks();
	decoded->header.threadId = bb_get_current_thread_id();
	decoded->header.fileId = pathId;
	decoded->header.line = line;
}

static BB_INLINE void bb_send(bb_decoded_packet_t* decoded)
{
	u8 buf[BB_MAX_PACKET_BUFFER_SIZE];
	u16 serializedLen = bbpacket_serialize(decoded, buf + 2, sizeof(buf) - 2);
	if (serializedLen)
	{
		serializedLen += 2;
		buf[0] = (u8)(serializedLen >> 8);
		buf[1] = (u8)(serializedLen & 0xFF);
	}
	else
	{
		bb_error("bb_send failed to encode packet");
		return;
	}

	if (s_initial_buffer.cs.initialized && bbpacket_is_log_text_type(decoded->type))
	{
		bb_critical_section_lock(&s_initial_buffer.cs);
		if (s_initial_buffer.data != NULL)
		{
			if (s_initial_buffer.used + serializedLen <= s_initial_buffer.size)
			{
				memcpy((u8*)s_initial_buffer.data + s_initial_buffer.used, buf, serializedLen);
				s_initial_buffer.used += serializedLen;
			}
			else
			{
				bb_log("bb_send filled initial buffer of size %u - discarding", s_initial_buffer.size);
				s_initial_buffer.data = NULL;
				s_initial_buffer.size = 0u;
				s_initial_buffer.used = 0u;
				s_initial_buffer.state = kBBInitialBuffer_Done;
			}
		}
		bb_critical_section_unlock(&s_initial_buffer.cs);
	}

	if (s_bb_send_callback)
	{
		(*s_bb_send_callback)(s_bb_send_callback_context, decoded);
	}
	if (s_fp || s_bb_write_callback)
	{
		if (s_bb_write_callback)
		{
			(*s_bb_write_callback)(s_bb_write_callback_context, buf, serializedLen);
		}
		if (s_fp)
		{
			bb_file_write(s_fp, buf, serializedLen);
		}
	}
	bbcon_send_raw(&s_con, buf, serializedLen);
}

static const char* s_bbLogLevelNames[] = {
	"Log",
	"Warning",
	"Error",
	"Display",
	"SetColor",
	"VeryVerbose",
	"Verbose",
	"Fatal",
};
BB_CTASSERT(BB_ARRAYSIZE(s_bbLogLevelNames) == kBBLogLevel_Count);
const char* bb_get_log_level_name(bb_log_level_e logLevel, const char* defaultValue)
{
	if (logLevel >= 0 && logLevel < kBBLogLevel_Count)
		return s_bbLogLevelNames[logLevel];
	return defaultValue;
}

bb_log_level_e bb_get_log_level_from_name(const char* name)
{
	for (u32 i = 0; i < kBBLogLevel_Count; ++i)
	{
		if (bb_stricmp(s_bbLogLevelNames[i], name) == 0)
		{
			return (bb_log_level_e)i;
		}
	}
	return kBBLogLevel_Count;
}

bb_platform_e bb_platform(void)
{
#if BB_USING(BB_PLATFORM_WINDOWS)
	return kBBPlatform_Windows;
#elif BB_USING(BB_PLATFORM_LINUX)
	return kBBPlatform_Linux;
#elif BB_USING(BB_PLATFORM_ANDROID)
	return kBBPlatform_Android;
#elif BB_USING(BB_PLATFORM_ORBIS)
	return kBBPlatform_Orbis;
#elif BB_USING(BB_PLATFORM_DURANGO)
	return kBBPlatform_Durango;
#elif BB_USING(BB_PLATFORM_NX)
	return kBBPlatform_Nx;
#elif BB_USING(BB_PLATFORM_PROSPERO)
	return kBBPlatform_Prospero;
#elif BB_USING(BB_PLATFORM_SCARLETT)
	return kBBPlatform_Scarlett;
#else
	BB_ASSERT(false);
	return kBBPlatform_Unknown;
#endif
}

static const char* s_bb_platformNames[] = {
	"Unknown",  // kBBPlatform_Unknown,
	"Windows",  // kBBPlatform_Windows,
	"Linux",    // kBBPlatform_Linux,
	"Android",  // kBBPlatform_Android,
	"PS4",      // kBBPlatform_Orbis,
	"Xbox One", // kBBPlatform_Durango,
	"Switch",   // kBBPlatform_Nx,
	"Prospero", // kBBPlatform_Prospero,
	"Scarlett", // kBBPlatform_Scarlett,
};
BB_CTASSERT(BB_ARRAYSIZE(s_bb_platformNames) == kBBPlatform_Count);

const char* bb_platform_name(bb_platform_e platform)
{
	platform = (platform < kBBPlatform_Count) ? platform : kBBPlatform_Unknown;
	return s_bb_platformNames[platform];
}

static BB_INLINE void bb_send_directed(bb_decoded_packet_t* decoded, b32 bCallbacks, b32 bSocket, b32 bFile)
{
	if (s_bb_send_callback && bCallbacks)
	{
		(*s_bb_send_callback)(s_bb_send_callback_context, decoded);
	}
	if (s_fp || s_bb_write_callback)
	{
		u8 buf[BB_MAX_PACKET_BUFFER_SIZE];
		u16 serializedLen = bbpacket_serialize(decoded, buf + 2, sizeof(buf) - 2);
		if (serializedLen)
		{
			serializedLen += 2;
			buf[0] = (u8)(serializedLen >> 8);
			buf[1] = (u8)(serializedLen & 0xFF);
			if (s_bb_write_callback && bCallbacks)
			{
				(*s_bb_write_callback)(s_bb_write_callback_context, buf, serializedLen);
			}
			if (s_fp && bFile)
			{
				bb_file_write(s_fp, buf, serializedLen);
			}
		}
		else
		{
			bb_error("bb_send failed to encode packet");
		}
	}
	if (bSocket)
	{
		bbcon_send(&s_con, decoded);
	}
}

static void bb_send_ids(bb_ids_t* ids, b32 bCallbacks, b32 bSocket, b32 bFile)
{
	for (u32 i = 0; i < ids->count; ++i)
	{
		bb_id_t* id = ids->data + i;

		bb_decoded_packet_t decoded = { BB_EMPTY_INITIALIZER };
		decoded.type = id->packetType;
		decoded.header = id->header;
		decoded.packet.registerId.id = id->id;
		if (id->name)
		{
			bb_strncpy(decoded.packet.registerId.name, id->name, sizeof(decoded.packet.registerId.name));
		}
		bb_send_directed(&decoded, bCallbacks, bSocket, bFile);
	}
}

static void bb_send_thread_ids(bb_ids_t* ids, b32 bCallbacks, b32 bSocket, b32 bFile)
{
	for (u32 i = 0; i < ids->count; ++i)
	{
		bb_id_t* id = ids->data + i;

		bb_decoded_packet_t decoded = { BB_EMPTY_INITIALIZER };
		decoded.type = id->packetType;
		decoded.header = id->header;
		if (id->packetType == kBBPacketType_ThreadStart && id->name)
		{
			bb_strncpy(decoded.packet.threadStart.text, id->name, sizeof(decoded.packet.threadStart.text));
		}
		else if (id->packetType == kBBPacketType_ThreadName && id->name)
		{
			bb_strncpy(decoded.packet.threadName.text, id->name, sizeof(decoded.packet.threadName.text));
		}
		bb_send_directed(&decoded, bCallbacks, bSocket, bFile);
	}
}

static bb_decoded_packet_t bb_build_appinfo(void)
{
	bb_decoded_packet_t decoded;
	decoded.type = (s_applicationGroup[0] == '\0') ? kBBPacketType_AppInfo_v4 : kBBPacketType_AppInfo;
	decoded.header.timestamp = bb_current_ticks();
	decoded.header.threadId = bb_get_current_thread_id();
	decoded.header.fileId = 0;
	decoded.header.line = 0;
	decoded.packet.appInfo.initialTimestamp = decoded.header.timestamp;
	decoded.packet.appInfo.millisPerTick = bb_millis_per_tick();
	decoded.packet.appInfo.initFlags = g_bb_initFlags;
	decoded.packet.appInfo.platform = (u32)(bb_platform_e)bb_platform();
	decoded.packet.appInfo.microsecondsFromEpoch = bb_current_time_microseconds_from_epoch();
	bb_strncpy(decoded.packet.appInfo.applicationName, s_applicationName, sizeof(decoded.packet.appInfo.applicationName));
	bb_strncpy(decoded.packet.appInfo.applicationGroup, s_applicationGroup, sizeof(decoded.packet.appInfo.applicationGroup));
	return decoded;
}

static void bb_save_initial_appinfo(void)
{
	if (s_initial_buffer.cs.initialized)
	{
		bb_critical_section_lock(&s_initial_buffer.cs);
		s_initialAppInfo = bb_build_appinfo();
		bb_critical_section_unlock(&s_initial_buffer.cs);
	}
}

static void bb_send_initial(b32 bCallbacks, b32 bSocket, b32 bFile)
{
	BB_ASSERT(bbpacket_is_app_info_type(s_initialAppInfo.type));
	bb_send_directed(&s_initialAppInfo, bCallbacks, bSocket, bFile);

	bb_send_ids(&s_bb_pathIds, bCallbacks, bSocket, bFile);
	bb_send_ids(&s_bb_categoryIds, bCallbacks, bSocket, bFile);
	bb_send_thread_ids(&s_bb_threadIds, bCallbacks, bSocket, bFile);

	 if (s_initial_buffer.cs.initialized)
	{
		bb_critical_section_lock(&s_initial_buffer.cs);
		if (s_initial_buffer.data != NULL && s_initial_buffer.used > 0)
		{
			bbcon_send_raw(&s_con, (u8*)s_initial_buffer.data + s_initial_buffer.start, s_initial_buffer.used - s_initial_buffer.start);
		}
		bb_critical_section_unlock(&s_initial_buffer.cs);
	}
}

void bb_init_file(const char* path)
{
	bb_init_locale();
	if (s_fp == BB_INVALID_FILE_HANDLE)
	{
		s_fp = bb_file_open_for_write(path);

		if (s_id_cs.initialized && bbpacket_is_app_info_type(s_initialAppInfo.type))
		{
			bb_critical_section_lock(&s_id_cs);
			bb_send_initial(false, false, true);
			bb_critical_section_unlock(&s_id_cs);
			s_bFileSentAppInfo = true;
		}
	}
}

#if BB_COMPILE_WIDECHAR
void bb_init_file_w(const bb_wchar_t* path)
{
	bb_init_locale();
	bb_init_file(bb_wcstombcs(path));
}
#endif // #if BB_COMPILE_WIDECHAR

uint32_t bb_get_server_ip(void)
{
	return s_serverIp;
}

uint16_t bb_get_server_port(void)
{
	return s_serverPort;
}

void bb_disconnect(void)
{
	if (bbcon_is_connected(&s_con))
	{
		bbcon_disconnect(&s_con);
	}
}

void bb_connect_direct(uint32_t targetIp, uint16_t targetPort, const void* payload, uint32_t payloadBytes)
{
	if (!s_id_cs.initialized)
		return;

	bb_critical_section_lock(&s_id_cs);

	b32 bCallbacks = !s_bCallbackSentAppInfo;
	s_bCallbackSentAppInfo = true;
	b32 bFile = !s_bFileSentAppInfo;
	s_bFileSentAppInfo = true;
	b32 bSocket = false;
	bb_disconnect();
	s_serverIp = targetIp;
	s_serverPort = targetPort;
	if (bbcon_connect_client_async_ipv4(&s_con, targetIp, targetPort))
	{
		while (bbcon_is_connecting(&s_con))
		{
			bbcon_tick_connecting(&s_con);
		}
		if (bbcon_is_connected(&s_con))
		{
			bSocket = true;

			if (payload && payloadBytes)
			{
				bbcon_send_raw(&s_con, payload, payloadBytes);
			}
		}
	}
	bb_send_initial(bCallbacks, bSocket, bFile);

	bb_critical_section_unlock(&s_id_cs);
}

b32 bb_connect_sockaddr(const struct sockaddr* discoveryAddr, size_t discoveryAddrSize)
{
	if (!s_id_cs.initialized)
		return false;

	bb_critical_section_lock(&s_id_cs);

	b32 bCallbacks = !s_bCallbackSentAppInfo;
	s_bCallbackSentAppInfo = true;
	b32 bFile = !s_bFileSentAppInfo;
	s_bFileSentAppInfo = true;
	b32 bSocket = false;

	bb_disconnect();
	bb_discovery_result_t discovery = bb_discovery_client_start(s_applicationName, s_sourceApplicationName, s_deviceCode, s_sourceIp, discoveryAddr, discoveryAddrSize);
	if (discovery.success)
	{
		if (discovery.serverAddr.ss_family == AF_INET)
		{
			s_serverIp = BB_S_ADDR_UNION(*(const struct sockaddr_in*)&discovery.serverAddr);
		}
		else
		{
			s_serverIp = 0;
		}
		s_serverPort = bbnet_get_port_from_sockaddr((const struct sockaddr*)&discovery.serverAddr);
		if (bbcon_connect_client_async(&s_con, (const struct sockaddr*)&discovery.serverAddr, sizeof(discovery.serverAddr)))
		{
			while (bbcon_is_connecting(&s_con))
			{
				bbcon_tick_connecting(&s_con);
			}
			if (bbcon_is_connected(&s_con))
			{
				bSocket = true;
			}
		}
	}

	bb_send_initial(bCallbacks, bSocket, bFile);

	bb_critical_section_unlock(&s_id_cs);

	return bSocket;
}

b32 bb_connect_ipv4(uint32_t discoveryIp, uint16_t discoveryPort)
{
	struct sockaddr_in addr = { BB_EMPTY_INITIALIZER };
	addr.sin_family = AF_INET;
	BB_S_ADDR_UNION(addr) = (discoveryIp) ? htonl(discoveryIp) : INADDR_BROADCAST;
	addr.sin_port = htons(discoveryPort ? discoveryPort : BB_DISCOVERY_PORT);
	return bb_connect_sockaddr((const struct sockaddr*)&addr, sizeof(addr));
}

int32_t bb_connect_str(const char* discoveryAddr, uint16_t discoveryPort)
{
	struct sockaddr_storage addr = { BB_EMPTY_INITIALIZER };
	addr.ss_family = (strchr(discoveryAddr, '.') == 0) ? AF_INET6 : AF_INET;
	if (discoveryAddr)
	{
#if BB_USING(BB_FEATURE_IPV6)
		if (addr.ss_family == AF_INET6)
		{
			struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&addr;
			if (inet_pton(AF_INET6, discoveryAddr, &addr6->sin6_addr) != 1)
			{
				return false;
			}
			addr6->sin6_port = htons(discoveryPort ? discoveryPort : BB_DISCOVERY_PORT);
		}
		else
#endif // #if BB_USING(BB_FEATURE_IPV6)
		{
			struct sockaddr_in* addr4 = (struct sockaddr_in*)&addr;
			if (inet_pton(AF_INET, discoveryAddr, &addr4->sin_addr) != 1)
			{
				return false;
			}
			addr4->sin_port = htons(discoveryPort ? discoveryPort : BB_DISCOVERY_PORT);
		}
	}
	else
	{
		return false;
	}
	return bb_connect_sockaddr((const struct sockaddr*)&addr, sizeof(addr));
}

void bb_connect(uint32_t discoveryIp, uint16_t discoveryPort)
{
	if (bb_connect_ipv4(discoveryIp, discoveryPort))
		return;

	if (discoveryIp != 0)
	{
		if (bb_connect_ipv4(0, discoveryPort))
			return;
	}
}

void bb_init_critical_sections(void)
{
	if (!s_id_cs.initialized)
	{
		bb_critical_section_init(&s_id_cs);
	}
	if (!s_initial_buffer.cs.initialized)
	{
		bb_critical_section_init(&s_initial_buffer.cs);
	}
}

void bb_init(const char* applicationName, const char* sourceApplicationName, const char* deviceCode, uint32_t sourceIp, bb_init_flags_t initFlags)
{
	bb_init_locale();
	if (!deviceCode)
	{
		deviceCode = "";
	}
	if (!sourceApplicationName)
	{
		sourceApplicationName = "";
	}
	bb_strncpy(s_deviceCode, deviceCode, sizeof(s_deviceCode));
	bb_strncpy(s_applicationName, applicationName, sizeof(s_applicationName));
	bb_strncpy(s_sourceApplicationName, sourceApplicationName, sizeof(s_sourceApplicationName));
	g_bb_initFlags = initFlags;
	bb_init_critical_sections();
	bb_log_init();
	bbnet_init();
	bbcon_init(&s_con);
	s_con.flags |= kBBCon_Blackbox;
	s_sourceIp = sourceIp;
	bb_save_initial_appinfo();

	if (s_id_cs.initialized)
	{
		bb_critical_section_lock(&s_id_cs);
		if (s_fp != NULL && !s_bFileSentAppInfo)
		{
			bb_send_initial(false, false, true);
			s_bFileSentAppInfo = true;
		}
		bb_critical_section_unlock(&s_id_cs);
	}

	if ((g_bb_initFlags & kBBInitFlag_NoConnect) != 0)
		return; // no connect, with or without discovery

	if ((g_bb_initFlags & kBBInitFlag_NoDiscovery) != 0)
	{
		bb_connect((127 << 24) | 1, 0); // no discovery, so only try connecting to localhost
	}
	else
	{
		bb_connect(0, 0); // full discovery
	}
}

#if BB_COMPILE_WIDECHAR
void bb_init_w(const bb_wchar_t* applicationName, const bb_wchar_t* sourceApplicationName, const bb_wchar_t* deviceCode, uint32_t sourceIp, bb_init_flags_t initFlags)
{
	bb_init_locale();
	if (!sourceApplicationName)
	{
		sourceApplicationName = BB_WCHARS("");
	}
	bb_init(bb_wcstombcs(applicationName), bb_wcstombcs(sourceApplicationName), bb_wcstombcs(deviceCode), sourceIp, initFlags);
}
#endif // #if BB_COMPILE_WIDECHAR

static void bb_free_ids(bb_ids_t *ids)
{
	for (u32 i = 0; i < ids->count; ++i)
	{
		bb_id_t* id = ids->data + i;
		if (id->name)
		{
			bb_free(id->name);
		}
	}
	bba_free(*ids);
}

void bb_shutdown(const char* file, int line)
{
	uint32_t bb_path_id = 0;
	bb_resolve_path_id(file, &bb_path_id, (uint32_t)line);
	bb_thread_end(bb_path_id, (u32)line);
	if (s_fp != BB_INVALID_FILE_HANDLE)
	{
		bb_file_close(s_fp);
		s_fp = BB_INVALID_FILE_HANDLE;
	}
	bbcon_flush(&s_con);
	bbcon_shutdown(&s_con);
	bbnet_shutdown();
	bb_log_shutdown();
	bb_critical_section_shutdown(&s_id_cs);
	bb_free_ids(&s_bb_categoryIds);
	bb_free_ids(&s_bb_pathIds);
	bb_free_ids(&s_bb_threadIds);
	if (s_bb_trace_packet_buffer)
	{
		bb_free(s_bb_trace_packet_buffer);
		s_bb_trace_packet_buffer = NULL;
	}
	bb_shutdown_locale();
	bb_critical_section_shutdown(&s_initial_buffer.cs);
	memset(&s_initial_buffer, 0, sizeof(s_initial_buffer));
}

void bb_set_initial_buffer(void* buffer, uint32_t bufferSize)
{
	if (s_con.cs.initialized && buffer)
	{
		bb_error("bb initial buffer can only be set before bb_init");
		BB_ASSERT(false);
		buffer = NULL;
		bufferSize = 0;
	}

	if (!s_initial_buffer.cs.initialized)
	{
		bb_critical_section_init(&s_initial_buffer.cs);
	}

	bb_critical_section_lock(&s_initial_buffer.cs);
	s_initial_buffer.data = buffer;
	s_initial_buffer.size = bufferSize;
	if (bufferSize > 0)
	{
		s_initial_buffer.state = kBBInitialBuffer_Set;
	}
	else
	{
		s_initial_buffer.state = kBBInitialBuffer_Done;
	}
	bb_critical_section_unlock(&s_initial_buffer.cs);
}

void bb_pre_init_set_applicationGroup(const char* applicationGroup)
{
	if (s_con.cs.initialized)
	{
		bb_error("bb application group can only be set before bb_init");
		BB_ASSERT(false);
	}
	else
	{
		bb_strncpy(s_applicationGroup, applicationGroup, sizeof(s_applicationGroup));
	}
}

void bb_enable_stored_thread_ids(int store)
{
	s_bDisableStoredThreadIds = !store;
}

int bb_is_connected(void)
{
	return bbcon_is_connected(&s_con);
}

void bb_set_send_interval_ms(uint64_t sendInterval)
{
	s_con.sendInterval = sendInterval;
}

void bb_tick(void)
{
	bb_decoded_packet_t decoded;
	bbcon_tick(&s_con);
	if (s_fp != BB_INVALID_FILE_HANDLE || s_bb_flush_callback)
	{
		u64 now = bb_current_time_ms();
		if (now > s_lastFileFlushTime + kBBFile_FlushIntervalMillis)
		{
			s_lastFileFlushTime = now;
			if (s_bb_flush_callback)
			{
				(*s_bb_flush_callback)(s_bb_flush_callback_context);
			}
			if (s_fp != BB_INVALID_FILE_HANDLE)
			{
				bb_file_flush(s_fp);
			}
		}
	}
	while (bbcon_decodePacket(&s_con, &decoded))
	{
		// handle server->client packet here - callback to application
		if (s_bb_incoming_packet_handler)
		{
			(*s_bb_incoming_packet_handler)(&decoded, s_bb_incoming_packet_context);
		}
	}
}

void bb_flush(void)
{
	if (s_bb_flush_callback)
	{
		(*s_bb_flush_callback)(s_bb_flush_callback_context);
	}
	if (s_fp != BB_INVALID_FILE_HANDLE)
	{
		bb_file_flush(s_fp);
	}
	bbcon_flush(&s_con);
}

uint64_t bb_get_total_bytes_sent(void)
{
	uint64_t bytes = 0;
	if (bbcon_is_connected(&s_con))
	{
		bb_critical_section_lock(&s_con.cs);
		bytes = s_con.sentBytesTotal;
		bb_critical_section_unlock(&s_con.cs);
	}
	return bytes;
}

uint64_t bb_get_total_bytes_received(void)
{
	uint64_t bytes = 0;
	if (bbcon_is_connected(&s_con))
	{
		bb_critical_section_lock(&s_con.cs);
		bytes = s_con.receivedBytesTotal;
		bb_critical_section_unlock(&s_con.cs);
	}
	return bytes;
}

void bb_echo_to_stdout(void* context, bb_decoded_packet_t* decoded)
{
	BB_UNUSED(context);
	if (bbpacket_is_log_text_type(decoded->type))
	{
		switch (decoded->packet.logText.level)
		{
		case kBBLogLevel_Warning:
		case kBBLogLevel_Error:
		case kBBLogLevel_Fatal:
			fputs(decoded->packet.logText.text, stderr);
#if BB_USING(BB_PLATFORM_WINDOWS)
			OutputDebugStringA(decoded->packet.logText.text);
#endif
			break;

		case kBBLogLevel_Log:
		case kBBLogLevel_Display:
			fputs(decoded->packet.logText.text, stdout);
#if BB_USING(BB_PLATFORM_WINDOWS)
			OutputDebugStringA(decoded->packet.logText.text);
#endif
			break;

		default:
#if BB_USING(BB_PLATFORM_WINDOWS)
			OutputDebugStringA(decoded->packet.logText.text);
#endif
			break;
		}
	}
}

void bb_set_write_callback(bb_write_callback callback, void* context)
{
	s_bb_write_callback = callback;
	s_bb_write_callback_context = context;
}

void bb_set_flush_callback(bb_flush_callback callback, void* context)
{
	s_bb_flush_callback = callback;
	s_bb_flush_callback_context = context;
}

void bb_set_send_callback(bb_send_callback callback, void* context)
{
	s_bb_send_callback = callback;
	s_bb_send_callback_context = context;
}

void bb_set_incoming_packet_handler(bb_incoming_packet_handler handler, void* context)
{
	s_bb_incoming_packet_handler = handler;
	s_bb_incoming_packet_context = context;
}

int bb_send_raw_packet(bb_decoded_packet_t* decoded)
{
	if (decoded->type == kBBPacketType_ConsoleAutocompleteResponseHeader || decoded->type == kBBPacketType_ConsoleAutocompleteResponseEntry || decoded->type == kBBPacketType_UserToServer)
	{
		bb_fill_header(decoded, decoded->type, 0, 0);
		bb_send(decoded);
		return 1;
	}
	else
	{
		return 0;
	}
}

static void bb_thread_store_id_packet(bb_decoded_packet_t* decoded)
{
	if (s_bDisableStoredThreadIds)
		return;
	if (s_id_cs.initialized)
	{
		bb_critical_section_lock(&s_id_cs);
	}
	bb_id_t* newIdData = bba_add(s_bb_threadIds, 1);
	if (newIdData)
	{
		newIdData->packetType = decoded->type;
		newIdData->header = decoded->header;
		if (decoded->type == kBBPacketType_ThreadStart)
		{
			newIdData->name = bb_strdup(decoded->packet.threadStart.text);
		}
		else if (decoded->type == kBBPacketType_ThreadName)
		{
			newIdData->name = bb_strdup(decoded->packet.threadName.text);
		}
	}
	if (s_id_cs.initialized)
	{
		bb_critical_section_unlock(&s_id_cs);
	}
}

void bb_thread_start(uint32_t pathId, uint32_t line, const char* name)
{
	bb_init_critical_sections();
	bb_decoded_packet_t decoded;
	bb_fill_header(&decoded, kBBPacketType_ThreadStart, pathId, line);
	bb_strncpy(decoded.packet.threadStart.text, name, sizeof(decoded.packet.threadStart.text));
	bb_thread_store_id_packet(&decoded);
	bb_send(&decoded);
}

#if BB_COMPILE_WIDECHAR
void bb_thread_start_w(uint32_t pathId, uint32_t line, const bb_wchar_t* name)
{
	bb_thread_start(pathId, line, bb_wcstombcs(name));
}
#endif // #if BB_COMPILE_WIDECHAR

void bb_thread_set_name(uint32_t pathId, uint32_t line, const char* name)
{
	bb_init_critical_sections();
	bb_decoded_packet_t decoded;
	bb_fill_header(&decoded, kBBPacketType_ThreadName, pathId, line);
	bb_strncpy(decoded.packet.threadName.text, name, sizeof(decoded.packet.threadName.text));
	bb_thread_store_id_packet(&decoded);
	bb_send(&decoded);
}

#if BB_COMPILE_WIDECHAR
void bb_thread_set_name_w(uint32_t pathId, uint32_t line, const bb_wchar_t* name)
{
	bb_thread_set_name(pathId, line, bb_wcstombcs(name));
}
#endif // #if BB_COMPILE_WIDECHAR

void bb_thread_end(uint32_t pathId, uint32_t line)
{
	bb_decoded_packet_t decoded;
	bb_fill_header(&decoded, kBBPacketType_ThreadEnd, pathId, line);
	bb_send(&decoded);
	bb_thread_store_id_packet(&decoded);
	if (s_bb_trace_packet_buffer)
	{
		bb_free(s_bb_trace_packet_buffer);
		s_bb_trace_packet_buffer = NULL;
	}
}

void bb_start_frame_number(uint32_t pathId, uint32_t line, uint64_t frameNumber)
{
	bb_decoded_packet_t decoded;
	bb_fill_header(&decoded, kBBPacketType_FrameNumber, pathId, line);
	decoded.packet.frameNumber.frameNumber = frameNumber;
	bb_send(&decoded);
}

static u32 bb_find_id(const char* name, bb_ids_t* ids)
{
	u32 i;
	for (i = 0; i < ids->count; ++i)
	{
		bb_id_t* id = ids->data + i;
		if (id->name && !bb_stricmp(id->name, name))
		{
			return id->id;
		}
	}
	return 0;
}

static u32 bb_resolve_id(const char* name, bb_ids_t* ids, u32 pathId, u32 line, bb_packet_type_e packetType, size_t maxSize, b32 recurse)
{
	bb_decoded_packet_t decoded;
	u32 existing = bb_find_id((name = (name ? name : "")), ids);
	if (existing)
	{
		return existing;
	}
	else
	{
		// need to add parent categories also on the client here so they can get proper ids
		if (packetType == kBBPacketType_CategoryId && recurse)
		{
			char categoryBuf[kBBSize_Category];
			char* c = categoryBuf;
			const char* s = name;
			while (*s && c - categoryBuf < kBBSize_Category)
			{
				if (s[0] == ':' && s[1] == ':')
				{
					*c = '\0';
					bb_resolve_id(categoryBuf, ids, pathId, line, packetType, maxSize, false);
					*c++ = *s++;
				}
				*c++ = *s++;
			}
		}

		{
			u32 newId = ++ids->lastId;
			bb_id_t* newIdData = bba_add(*ids, 1);
			// u32 tmp;
			// for(tmp = 0; tmp < 1000; ++tmp) {
			//	newIdData = bba_add(*ids, 1);
			// }
			bb_fill_header(&decoded, packetType, (pathId) ? pathId : newId, line);
			if (newIdData)
			{
				newIdData->header = decoded.header;
				newIdData->id = newId;
				newIdData->packetType = packetType;
				newIdData->name = bb_strdup(name);
			}
			decoded.packet.registerId.id = newId;
			bb_strncpy(decoded.packet.registerId.name, name, BB_MIN(maxSize, sizeof(decoded.packet.registerId.name)));
			bb_send(&decoded);
			return newId;
		}
	}
}

uint32_t bb_resolve_ids(const char* path, const char* category, uint32_t* pathId, uint32_t* categoryId, uint32_t line)
{
	if (!s_id_cs.initialized)
		return 0;
	bb_critical_section_lock(&s_id_cs);
	if (!*pathId)
	{
		*pathId = bb_resolve_id(path, &s_bb_pathIds, 0, line, kBBPacketType_FileId, ~0U, false);
	}
	if (!*categoryId)
	{
		*categoryId = bb_resolve_id(category, &s_bb_categoryIds, *pathId, line, kBBPacketType_CategoryId, kBBSize_Category, true);
	}
	bb_critical_section_unlock(&s_id_cs);
	return 1;
}

#if BB_COMPILE_WIDECHAR
uint32_t bb_resolve_ids_w(const char* path, const bb_wchar_t* category, uint32_t* pathId, uint32_t* categoryId, uint32_t line)
{
	return bb_resolve_ids(path, bb_wcstombcs(category), pathId, categoryId, line);
}
#endif // #if BB_COMPILE_WIDECHAR

void bb_resolve_path_id(const char* path, uint32_t* pathId, uint32_t line)
{
	if (!s_id_cs.initialized)
		return;
	bb_critical_section_lock(&s_id_cs);
	if (!*pathId)
	{
		*pathId = bb_resolve_id(path, &s_bb_pathIds, 0, line, kBBPacketType_FileId, ~0U, false);
	}
	bb_critical_section_unlock(&s_id_cs);
}

static bb_color_t bb_resolve_color_str(const char* str)
{
	// clang-format off
	if (!strncmp("0000", str, 4)) return kBBColor_UE4_Black;
	if (!strncmp("1000", str, 4)) return kBBColor_UE4_DarkRed;
	if (!strncmp("0100", str, 4)) return kBBColor_UE4_DarkGreen;
	if (!strncmp("0010", str, 4)) return kBBColor_UE4_DarkBlue;
	if (!strncmp("1100", str, 4)) return kBBColor_UE4_DarkYellow;
	if (!strncmp("0110", str, 4)) return kBBColor_UE4_DarkCyan;
	if (!strncmp("1010", str, 4)) return kBBColor_UE4_DarkPurple;
	if (!strncmp("1110", str, 4)) return kBBColor_UE4_DarkWhite;
	if (!strncmp("1001", str, 4)) return kBBColor_UE4_Red;
	if (!strncmp("0101", str, 4)) return kBBColor_UE4_Green;
	if (!strncmp("0011", str, 4)) return kBBColor_UE4_Blue;
	if (!strncmp("1101", str, 4)) return kBBColor_UE4_Yellow;
	if (!strncmp("0111", str, 4)) return kBBColor_UE4_Cyan;
	if (!strncmp("1011", str, 4)) return kBBColor_UE4_Purple;
	if (!strncmp("1111", str, 4)) return kBBColor_UE4_White;
	// clang-format on
	return kBBColor_Default;
}

static void bb_resolve_and_set_colors(const char* str)
{
	bb_color_t bgColor = kBBColor_Default;
	bb_color_t fgColor = kBBColor_Default;
	size_t len = strlen(str);
	if (len >= 8)
	{
		fgColor = bb_resolve_color_str(str);
		bgColor = bb_resolve_color_str(str + 4);
	}
	else if (len >= 4)
	{
		fgColor = bb_resolve_color_str(str);
	}
	bb_set_color(fgColor, bgColor);
}

static void bb_trace_send(bb_decoded_packet_t* decoded, size_t textlen)
{
	if (textlen >= kBBSize_LogText)
	{
		char* text = decoded->packet.logText.text;
		size_t preTextSize = (size_t)(text - (char*)decoded);
		while (textlen >= kBBSize_LogText)
		{
			bb_decoded_packet_t partial;
			memcpy(&partial, decoded, preTextSize);
			bb_strncpy(partial.packet.logText.text, text, kBBSize_LogText);
			partial.type = kBBPacketType_LogTextPartial;
			bb_send(&partial);
			text += kBBSize_LogText - 1;
			textlen -= kBBSize_LogText - 1;
		}

		bb_decoded_packet_t remainder;
		memcpy(&remainder, decoded, preTextSize);
		bb_strncpy(remainder.packet.logText.text, text, textlen + 1);
		bb_send(&remainder);
	}
	else
	{
		bb_send(decoded);
	}
}

typedef struct bb_trace_builder_s
{
	bb_decoded_packet_t* decoded;
	size_t textOffset;
	size_t textBufferSize;
	char* textStart;
} bb_trace_builder_t;

static b32 bb_trace_begin(bb_trace_builder_t* builder, uint32_t pathId, uint32_t line)
{
	if (!s_bb_trace_packet_buffer)
	{
		s_bb_trace_packet_buffer = (bbtraceBuffer_t*)bb_malloc(sizeof(*s_bb_trace_packet_buffer));
		if (!s_bb_trace_packet_buffer)
		{
			return false;
		}
	}
	builder->decoded = (bb_decoded_packet_t*)s_bb_trace_packet_buffer->packetBuffer;
	builder->textOffset = (size_t)(builder->decoded->packet.logText.text - (char*)builder->decoded);
	builder->textBufferSize = sizeof(s_bb_trace_packet_buffer->packetBuffer) - builder->textOffset;
	builder->textStart = s_bb_trace_packet_buffer->packetBuffer + sizeof(s_bb_trace_packet_buffer->packetBuffer) - builder->textBufferSize;
	bb_trace_partial_end();
	bb_fill_header(builder->decoded, kBBPacketType_LogText, pathId, line);
	return true;
}

static void bb_trace_end(bb_trace_builder_t* builder, int len, uint32_t categoryId, bb_log_level_e level, s32 pieInstance)
{
	int maxLen = (int)builder->textBufferSize - 2;
	len = (len < 0 || len > maxLen) ? maxLen : len;
	if (len == 0 || builder->textStart[len - 1] != '\n')
	{
		builder->textStart[len++] = '\n';
	}
	builder->textStart[len] = '\0';
	if (level == kBBLogLevel_SetColor)
	{
		bb_resolve_and_set_colors(builder->decoded->packet.logText.text);
	}
	else
	{
		builder->decoded->packet.logText.categoryId = categoryId;
		builder->decoded->packet.logText.level = (u32)(bb_log_level_e)level;
		builder->decoded->packet.logText.pieInstance = pieInstance;
		builder->decoded->packet.logText.colors = s_bb_colors;
		bb_trace_send(builder->decoded, (size_t)len);
	}
}

static void bb_trace_va(uint32_t pathId, uint32_t line, uint32_t categoryId, bb_log_level_e level, s32 pieInstance, const char* fmt, va_list args)
{
	bb_trace_builder_t builder = { BB_EMPTY_INITIALIZER };
	if (!bb_trace_begin(&builder, pathId, line))
	{
		return;
	}
	int len = vsnprintf(builder.textStart, builder.textBufferSize, fmt, args);
	bb_trace_end(&builder, len, categoryId, level, pieInstance);
}

void bb_trace(uint32_t pathId, uint32_t line, uint32_t categoryId, bb_log_level_e level, s32 pieInstance, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	bb_trace_va(pathId, line, categoryId, level, pieInstance, fmt, args);
	va_end(args);
}

#if BB_COMPILE_WIDECHAR
typedef struct bb_trace_builder_w_s
{
	bb_decoded_packet_t* decoded;
	size_t textBufferSize;
	size_t wstrSize;
	bb_wchar_t* wstr;
} bb_trace_builder_w_t;

static b32 bb_trace_begin_w(bb_trace_builder_w_t* builder, uint32_t pathId, uint32_t line)
{
	if (!s_bb_trace_packet_buffer)
	{
		s_bb_trace_packet_buffer = (bbtraceBuffer_t*)bb_malloc(sizeof(*s_bb_trace_packet_buffer));
		if (!s_bb_trace_packet_buffer)
		{
			return false;
		}
	}
	builder->decoded = (bb_decoded_packet_t*)s_bb_trace_packet_buffer->packetBuffer;
	builder->textBufferSize = sizeof(s_bb_trace_packet_buffer->packetBuffer) - sizeof(bb_decoded_packet_t) + kBBSize_LogText;
	builder->wstr = s_bb_trace_packet_buffer->wideBuffer;
	builder->wstrSize = BB_ARRAYSIZE(s_bb_trace_packet_buffer->wideBuffer);
	bb_trace_partial_end();
	bb_fill_header(builder->decoded, kBBPacketType_LogText, pathId, line);
	return true;
}

static void bb_trace_end_w(bb_trace_builder_w_t* builder, int len, uint32_t categoryId, bb_log_level_e level, s32 pieInstance)
{
	int maxLen = (int)builder->wstrSize - 2;
	len = (len < 0 || len > maxLen) ? maxLen : len;
	if (builder->wstr[len - 1] != L'\n')
	{
		builder->wstr[len++] = L'\n';
	}
	builder->wstr[len] = L'\0';
	size_t numCharsConverted = 0;
	bb_wcstombcs_inline(builder->wstr, builder->decoded->packet.logText.text, builder->textBufferSize, &numCharsConverted);
	if (level == kBBLogLevel_SetColor)
	{
		bb_resolve_and_set_colors(builder->decoded->packet.logText.text);
	}
	else
	{
		builder->decoded->packet.logText.categoryId = categoryId;
		builder->decoded->packet.logText.level = (u32)(bb_log_level_e)level;
		builder->decoded->packet.logText.pieInstance = pieInstance;
		builder->decoded->packet.logText.colors = s_bb_colors;
		bb_trace_send(builder->decoded, numCharsConverted);
	}
}

static void bb_trace_va_w(uint32_t pathId, uint32_t line, uint32_t categoryId, bb_log_level_e level, s32 pieInstance, const bb_wchar_t* fmt, va_list args)
{
	bb_trace_builder_w_t builder = { BB_EMPTY_INITIALIZER };
	if (!bb_trace_begin_w(&builder, pathId, line))
	{
		return;
	}
#if defined(BB_WIDE_CHAR16) && BB_WIDE_CHAR16
	int len = bb_vswprintf(builder.wstr, builder.wstrSize, fmt, args);
#else
	int len = vswprintf(builder.wstr, builder.wstrSize, fmt, args);
#endif
	bb_trace_end_w(&builder, len, categoryId, level, pieInstance);
}
#endif // #if BB_COMPILE_WIDECHAR

#if BB_COMPILE_WIDECHAR
void bb_trace_w(uint32_t pathId, uint32_t line, uint32_t categoryId, bb_log_level_e level, s32 pieInstance, const bb_wchar_t* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	bb_trace_va_w(pathId, line, categoryId, level, pieInstance, fmt, args);
	va_end(args);
}
#endif // #if BB_COMPILE_WIDECHAR

void bb_trace_dynamic(const char* path, uint32_t line, const char* category, bb_log_level_e level, s32 pieInstance, const char* fmt, ...)
{
	va_list args;
	uint32_t pathId = 0;
	uint32_t categoryId = 0;
	bb_resolve_ids(path, category, &pathId, &categoryId, line);
	va_start(args, fmt);
	bb_trace_va(pathId, line, categoryId, level, pieInstance, fmt, args);
	va_end(args);
}

void bb_trace_dynamic_preformatted_range(const char* path, uint32_t line, const char* category, bb_log_level_e level, int32_t pieInstance, const char* preformatted, const char* preformatted_end)
{
	uint32_t pathId = 0;
	uint32_t categoryId = 0;
	bb_resolve_ids(path, category, &pathId, &categoryId, line);

	size_t len = (preformatted_end && preformatted_end > preformatted) ? (size_t)(preformatted_end - preformatted) : strlen(preformatted);
	if (len < kBBSize_LogText)
	{
		bb_decoded_packet_t decoded = { BB_EMPTY_INITIALIZER };
		bb_fill_header(&decoded, kBBPacketType_LogText, pathId, line);
		decoded.packet.logText.categoryId = categoryId;
		decoded.packet.logText.level = (u32)(bb_log_level_e)level;
		decoded.packet.logText.pieInstance = pieInstance;
		decoded.packet.logText.colors = s_bb_colors;
		memcpy(&decoded.packet.logText.text, preformatted, len);
		decoded.packet.logText.text[len] = '\0';
		if (decoded.packet.logText.level == kBBLogLevel_SetColor)
		{
			bb_resolve_and_set_colors(decoded.packet.logText.text);
		}
		else
		{
			bb_send(&decoded);
		}
	}
	else
	{
		bb_trace_partial_preformatted(path, line, category, level, pieInstance, preformatted, preformatted_end);
		bb_trace_partial_end();
	}
}

void bb_trace_dynamic_preformatted(const char* path, uint32_t line, const char* category, bb_log_level_e level, int32_t pieInstance, const char* preformatted)
{
	bb_trace_dynamic_preformatted_range(path, line, category, level, pieInstance, preformatted, NULL);
}

#if BB_COMPILE_WIDECHAR
void bb_trace_dynamic_w(const char* path, uint32_t line, const bb_wchar_t* category, bb_log_level_e level, int32_t pieInstance, const bb_wchar_t* fmt, ...)
{
	va_list args;
	uint32_t pathId = 0;
	uint32_t categoryId = 0;
	bb_resolve_ids_w(path, category, &pathId, &categoryId, line);
	va_start(args, fmt);
	bb_trace_va_w(pathId, line, categoryId, level, pieInstance, fmt, args);
	va_end(args);
}
#endif // #if BB_COMPILE_WIDECHAR

#if BB_COMPILE_WIDECHAR
void bb_trace_dynamic_preformatted_w(const char* path, uint32_t line, const bb_wchar_t* category, bb_log_level_e level, int32_t pieInstance, const bb_wchar_t* preformatted)
{
#if defined(BB_WIDE_CHAR16) && BB_WIDE_CHAR16
	bb_trace_dynamic_w(path, line, category, level, pieInstance, u"%s", preformatted);
#else  // #if defined(BB_WIDE_CHAR16) && BB_WIDE_CHAR16
	uint32_t pathId = 0;
	uint32_t categoryId = 0;
	bb_resolve_ids_w(path, category, &pathId, &categoryId, line);

	bb_trace_builder_w_t builder = { BB_EMPTY_INITIALIZER };
	if (!bb_trace_begin_w(&builder, pathId, line))
	{
		return;
	}
	int len = (int)bb_wstrncpy(builder.wstr, preformatted, builder.wstrSize);
	bb_trace_end_w(&builder, len, categoryId, level, pieInstance);
#endif // #else // #if defined(BB_WIDE_CHAR16) && BB_WIDE_CHAR16
}
#endif // #if BB_COMPILE_WIDECHAR

typedef struct bb_partial_log_builder_s
{
	bb_decoded_packet_t decoded;
	uint32_t pathId;
	uint32_t line;
	int32_t len;
	int32_t partialPacketsSent;
} bb_partial_log_builder_t;
static bb_thread_local bb_partial_log_builder_t s_bb_partial;

void bb_trace_partial_end(void)
{
	if (s_bb_partial.len || s_bb_partial.partialPacketsSent)
	{
		int len = s_bb_partial.len;
		int maxLen;
		bb_decoded_packet_t* decoded = &s_bb_partial.decoded;
		bb_fill_header(decoded, kBBPacketType_LogText, s_bb_partial.pathId, s_bb_partial.line);
		maxLen = sizeof(decoded->packet.logText.text) - 1;
		len = (len < 0 || len > maxLen) ? maxLen : len;
		decoded->packet.logText.text[len] = '\0';
		decoded->packet.logText.colors = s_bb_colors;
		s_bb_partial.len = 0;
		s_bb_partial.partialPacketsSent = 0;
		if (decoded->packet.logText.level == kBBLogLevel_SetColor)
		{
			bb_resolve_and_set_colors(decoded->packet.logText.text);
		}
		else
		{
			bb_send(decoded);
		}
	}
}

static void bb_trace_partial_packet(void)
{
	if (s_bb_partial.len)
	{
		int len = s_bb_partial.len;
		int maxLen;
		bb_decoded_packet_t* decoded = &s_bb_partial.decoded;
		bb_fill_header(decoded, kBBPacketType_LogTextPartial, s_bb_partial.pathId, s_bb_partial.line);
		maxLen = sizeof(decoded->packet.logText.text) - 1;
		len = (len < 0 || len > maxLen) ? maxLen : len;
		decoded->packet.logText.text[len] = '\0';
		decoded->packet.logText.colors = s_bb_colors;
		if (decoded->packet.logText.level == kBBLogLevel_SetColor)
		{
			bb_trace_partial_end(); // calls bb_resolve_and_set_colors(decoded->packet.logText.text) internally
		}
		else
		{
			bb_send(decoded);
			s_bb_partial.len = 0;
			++s_bb_partial.partialPacketsSent;
		}
	}
}

void bb_trace_partial(const char* path, uint32_t line, const char* category, bb_log_level_e level, int32_t pieInstance, const char* fmt, ...)
{
	int textLen, i;
	va_list args;
	uint32_t pathId = 0;
	uint32_t categoryId = 0;
	char text[kBBSize_LogText];
	bb_decoded_packet_t* decoded = &s_bb_partial.decoded;
	bb_resolve_ids(path, category, &pathId, &categoryId, line);

	if ((s_bb_partial.len || s_bb_partial.partialPacketsSent) &&
	    ((u32)level != decoded->packet.logText.level ||
	     categoryId != decoded->packet.logText.categoryId ||
	     pieInstance != decoded->packet.logText.pieInstance))
	{
		bb_trace_partial_end();
	}

	s_bb_partial.pathId = pathId;
	s_bb_partial.line = line;
	decoded->packet.logText.level = (u32)(bb_log_level_e)level;
	decoded->packet.logText.categoryId = categoryId;
	decoded->packet.logText.pieInstance = pieInstance;

	va_start(args, fmt);
	textLen = vsnprintf(text, sizeof(text), fmt, args);
	va_end(args);
	if (textLen < 0)
	{
		textLen = sizeof(text) - 1;
	}

	for (i = 0; i < textLen; ++i)
	{
		char c = text[i];
		if (s_bb_partial.len >= (int)(sizeof(text) - 1))
		{
			bb_trace_partial_packet();

			// help out vs2017 static analysis - bb_trace_partial_packet() does this,
			// but it can't figure that out, so it thinks we could write past the
			// end of decoded->packet.logText.text.
			s_bb_partial.len = 0;
		}
		decoded->packet.logText.text[s_bb_partial.len++] = c;
	}
	if (textLen)
	{
		bb_trace_partial_packet();
	}
	decoded->packet.logText.text[s_bb_partial.len] = '\0';
}

void bb_trace_partial_preformatted(const char* path, uint32_t line, const char* category, bb_log_level_e level, int32_t pieInstance, const char* preformatted, const char* preformatted_end)
{
	if (!preformatted || !*preformatted)
		return;

	uint32_t pathId = 0;
	uint32_t categoryId = 0;
	bb_decoded_packet_t* decoded = &s_bb_partial.decoded;
	bb_resolve_ids(path, category, &pathId, &categoryId, line);

	if ((s_bb_partial.len || s_bb_partial.partialPacketsSent) &&
	    ((u32)level != decoded->packet.logText.level ||
	     categoryId != decoded->packet.logText.categoryId ||
	     pieInstance != decoded->packet.logText.pieInstance))
	{
		bb_trace_partial_end();
	}

	s_bb_partial.pathId = pathId;
	s_bb_partial.line = line;
	decoded->packet.logText.level = (u32)(bb_log_level_e)level;
	decoded->packet.logText.categoryId = categoryId;
	decoded->packet.logText.pieInstance = pieInstance;

	size_t textLen = (preformatted_end && preformatted_end > preformatted) ? (size_t)(preformatted_end - preformatted) : strlen(preformatted);

	for (size_t i = 0; i < textLen; ++i)
	{
		char c = preformatted[i];
		if (s_bb_partial.len >= (int)(sizeof(decoded->packet.logText.text) - 1))
		{
			bb_trace_partial_packet();

			// help out vs2017 static analysis - bb_trace_partial_packet() does this,
			// but it can't figure that out, so it thinks we could write past the
			// end of decoded->packet.logText.text.
			s_bb_partial.len = 0;
		}
		decoded->packet.logText.text[s_bb_partial.len++] = c;
	}
	if (textLen)
	{
		bb_trace_partial_packet();
	}
	decoded->packet.logText.text[s_bb_partial.len] = '\0';
}

#endif // #if BB_ENABLED
