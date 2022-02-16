// Copyright (c) 2012-2020 Matt Campbell
// MIT license (see License.txt)

#ifndef BB_H
#define BB_H

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(BB_ENABLED)
#define BB_ENABLED 1
#endif // #if !defined(BB_ENABLED)

#if !defined(BB_WIDECHAR)
#define BB_WIDECHAR 0
#endif // #if !defined(BB_WIDECHAR)

#if !defined(BB_COMPILE_WIDECHAR)
#define BB_COMPILE_WIDECHAR 1
#endif // #if !defined(BB_COMPILE_WIDECHAR)

#if BB_ENABLED

#if defined(_MSC_VER)
// sigh, VC 2019 16.4 throws C4668 in vcruntime.h, included from stdint.h
__pragma(warning(push));
__pragma(warning(disable : 4668));
#endif // defined(_MSC_VER)

#define __STDC_FORMAT_MACROS // explicitly request PRIu64 etc
#include <stdint.h>

#if defined(_MSC_VER)
__pragma(warning(pop));
#endif // defined(_MSC_VER)

#if BB_COMPILE_WIDECHAR
#if defined(BB_WIDE_CHAR16) && BB_WIDE_CHAR16
typedef char16_t bb_wchar_t;
#define BB_WCHARS_PASTE(x) u##x
#else // #if USING(BB_WIDE_CHAR16)
#include <wchar.h>
typedef wchar_t bb_wchar_t;
#define BB_WCHARS_PASTE(x) L##x
#endif // #else // #if USING(BB_WIDE_CHAR16)
#define BB_WCHARS(x) BB_WCHARS_PASTE(x)
#endif // #if BB_COMPILE_WIDECHAR

// buffer sizes, including null terminator
enum {
	kBBSize_ApplicationName = 64,
	kBBSize_ThreadName = 64,
	kBBSize_Category = 128,
	kBBSize_MaxPath = 2048,
	kBBSize_LogText = 2048,
	kBBSize_MachineName = 256,
	kBBSize_RecordingName = 256,
};

typedef enum {
	// Basic log levels
	kBBLogLevel_Log,
	kBBLogLevel_Warning,
	kBBLogLevel_Error,
	// Extra log levels for UE4
	kBBLogLevel_Display,
	kBBLogLevel_SetColor,
	kBBLogLevel_VeryVerbose,
	kBBLogLevel_Verbose,
	kBBLogLevel_Fatal,
	kBBLogLevel_Count
} bb_log_level_e;
const char *bb_get_log_level_name(bb_log_level_e logLevel, const char *defaultValue);

typedef enum {
	kBBInitFlag_None = 0x0,
	kBBInitFlag_NoOpenView = 0x1,
	kBBInitFlag_DebugInit = 0x2, // printf/OutputDebugString initial connection logging
	kBBInitFlag_ConsoleCommands = 0x4,
	kBBInitFlag_NoDiscovery = 0x8,
	kBBInitFlag_RecordingInfo = 0x10,
} bb_init_flag_e;
typedef uint32_t bb_init_flags_t;

typedef enum {
	kBBPlatform_Unknown,
	kBBPlatform_Windows,
	kBBPlatform_Linux,
	kBBPlatform_Android,
	kBBPlatform_Orbis,
	kBBPlatform_Durango,
	kBBPlatform_Nx,
	kBBPlatform_Prospero,
	kBBPlatform_Scarlett,
	kBBPlatform_Count
} bb_platform_e;

bb_platform_e bb_platform(void);
const char *bb_platform_name(bb_platform_e platform);

void bb_init(const char *applicationName, const char *sourceApplicationName, const char *deviceCode, uint32_t sourceIp, bb_init_flags_t initFlags);
void bb_init_file(const char *path);
void bb_shutdown(const char *file, int line);
void bb_set_initial_buffer(void *buffer, uint32_t bufferSize);
#if BB_WIDECHAR
void bb_init_w(const bb_wchar_t *applicationName, const bb_wchar_t *sourceApplicationName, const bb_wchar_t *deviceCode, uint32_t sourceIp, bb_init_flags_t initFlags);
void bb_init_file_w(const bb_wchar_t *path);
#endif // #if BB_WIDECHAR

void bb_connect(uint32_t discoveryIp, uint16_t discoveryPort);
void bb_connect_direct(uint32_t targetIp, uint16_t targetPort, const void *payload, uint32_t payloadBytes);
void bb_disconnect(void);
int bb_is_connected(void);
uint32_t bb_get_server_ip(void);
uint16_t bb_get_server_port(void);
void bb_set_send_interval_ms(uint64_t sendInterval);
void bb_tick(void);
void bb_flush(void);
uint64_t bb_get_total_bytes_sent(void);
uint64_t bb_get_total_bytes_received(void);

typedef void (*bb_write_callback)(void *context, void *data, uint32_t len);
void bb_set_write_callback(bb_write_callback callback, void *context);

typedef void (*bb_flush_callback)(void *context);
void bb_set_flush_callback(bb_flush_callback callback, void *context);

typedef struct bb_decoded_packet_s bb_decoded_packet_t;
typedef void (*bb_send_callback)(void *context, bb_decoded_packet_t *decoded);
void bb_set_send_callback(bb_send_callback callback, void *context);
void bb_echo_to_stdout(void *context, bb_decoded_packet_t *decoded);

typedef void (*bb_incoming_packet_handler)(const bb_decoded_packet_t *decoded, void *context);
void bb_set_incoming_packet_handler(bb_incoming_packet_handler handler, void *context);
#define BB_SET_INCOMING_PACKET_HANDLER(handler, context) bb_set_incoming_packet_handler((handler), (context))

void bb_thread_start(uint32_t pathId, uint32_t line, const char *name);
void bb_thread_set_name(uint32_t pathId, uint32_t line, const char *name);
void bb_thread_end(uint32_t pathId, uint32_t line);
#if BB_WIDECHAR
void bb_thread_start_w(uint32_t pathId, uint32_t line, const bb_wchar_t *name);
void bb_thread_set_name_w(uint32_t pathId, uint32_t line, const bb_wchar_t *name);
#endif // #if BB_WIDECHAR

uint32_t bb_resolve_ids(const char *path, const char *category, uint32_t *pathId, uint32_t *categoryId, uint32_t line);
void bb_resolve_path_id(const char *path, uint32_t *pathId, uint32_t line);
#if BB_WIDECHAR
uint32_t bb_resolve_ids_w(const char *path, const bb_wchar_t *category, uint32_t *pathId, uint32_t *categoryId, uint32_t line);
#endif // #if BB_WIDECHAR

void bb_trace(uint32_t pathId, uint32_t line, uint32_t categoryId, bb_log_level_e level, int32_t pieInstance, const char *fmt, ...);
void bb_trace_dynamic(const char *path, uint32_t line, const char *category, bb_log_level_e level, int32_t pieInstance, const char *fmt, ...);
void bb_trace_dynamic_preformatted(const char *path, uint32_t line, const char *category, bb_log_level_e level, int32_t pieInstance, const char *preformatted);
void bb_trace_dynamic_preformatted_range(const char *path, uint32_t line, const char *category, bb_log_level_e level, int32_t pieInstance, const char *preformatted, const char *preformatted_end);
void bb_trace_partial(const char *path, uint32_t line, const char *category, bb_log_level_e level, int32_t pieInstance, const char *fmt, ...);
void bb_trace_partial_preformatted(const char *path, uint32_t line, const char *category, bb_log_level_e level, int32_t pieInstance, const char *preformatted, const char *preformatted_end);
void bb_trace_partial_end(void);

#if BB_WIDECHAR
void bb_trace_w(uint32_t pathId, uint32_t line, uint32_t categoryId, bb_log_level_e level, int32_t pieInstance, const bb_wchar_t *fmt, ...);
void bb_trace_dynamic_w(const char *path, uint32_t line, const bb_wchar_t *category, bb_log_level_e level, int32_t pieInstance, const bb_wchar_t *fmt, ...);
void bb_trace_dynamic_preformatted_w(const char *path, uint32_t line, const bb_wchar_t *category, bb_log_level_e level, int32_t pieInstance, const bb_wchar_t *preformatted);
void bb_trace_partial_w(const char *path, uint32_t line, const bb_wchar_t *category, bb_log_level_e level, int32_t pieInstance, const bb_wchar_t *fmt, ...);
#endif // #if BB_WIDECHAR

#if BB_WIDECHAR
#define _BB_INIT_FUNC bb_init_w
#define _BB_INIT_FILE_FUNC bb_init_file_w
#define _BB_THREAD_START_FUNC bb_thread_start_w
#define _BB_THREAD_SET_NAME_FUNC bb_thread_set_name_w
#define _BB_RESOLVE_IDS_FUNC bb_resolve_ids_w
#define _BB_TRACE_FUNC bb_trace_w
#define _BB_TRACE_DYNAMIC_FUNC bb_trace_dynamic_w
#define _BB_TRACE_DYNAMIC_PREFORMATTED_FUNC bb_trace_dynamic_preformatted_w
#define _BB_TRACE_PARTIAL_FUNC bb_trace_partial_w
#else // #if BB_WIDECHAR
#define _BB_INIT_FUNC bb_init
#define _BB_INIT_FILE_FUNC bb_init_file
#define _BB_THREAD_START_FUNC bb_thread_start
#define _BB_THREAD_SET_NAME_FUNC bb_thread_set_name
#define _BB_RESOLVE_IDS_FUNC bb_resolve_ids
#define _BB_TRACE_FUNC bb_trace
#define _BB_TRACE_DYNAMIC_FUNC bb_trace_dynamic
#define _BB_TRACE_DYNAMIC_PREFORMATTED_FUNC bb_trace_dynamic_preformatted
#define _BB_TRACE_PARTIAL_FUNC bb_trace_partial
#endif // #else // #if BB_WIDECHAR

#define BB_PREINIT(logPath) _BB_INIT_FILE_FUNC(logPath)
#define BB_INIT(applicationName) _BB_INIT_FUNC(applicationName, 0, 0, 0, 0u)
#define BB_INIT_WITH_FLAGS(applicationName, flags) _BB_INIT_FUNC(applicationName, 0, 0, 0, flags)
#define BB_INIT_FROM_SOURCE(applicationName, sourceApplicationName, sourceIp) _BB_INIT_FUNC(applicationName, sourceApplicationName, 0, sourceIp, 0u)
#define BB_INIT_FROM_SOURCE_WITH_FLAGS(applicationName, sourceApplicationName, sourceIp, flags) _BB_INIT_FUNC(applicationName, sourceApplicationName, 0, sourceIp, flags)
#define BB_SHUTDOWN() bb_shutdown(__FILE__, __LINE__)

#define BB_IS_CONNECTED() bb_is_connected()
#define BB_TICK() bb_tick()
#define BB_FLUSH() bb_flush()

#define BB_THREAD_START(name)                                              \
	{                                                                      \
		static uint32_t bb_path_id = 0;                                    \
		if(!bb_path_id) {                                                  \
			bb_resolve_path_id(__FILE__, &bb_path_id, (uint32_t)__LINE__); \
		}                                                                  \
		_BB_THREAD_START_FUNC(bb_path_id, (uint32_t)__LINE__, name);       \
	}

#define BB_THREAD_SET_NAME(name)                                           \
	{                                                                      \
		static uint32_t bb_path_id = 0;                                    \
		if(!bb_path_id) {                                                  \
			bb_resolve_path_id(__FILE__, &bb_path_id, (uint32_t)__LINE__); \
		}                                                                  \
		_BB_THREAD_SET_NAME_FUNC(bb_path_id, (uint32_t)__LINE__, name);    \
	}

#define BB_THREAD_END()                                                    \
	{                                                                      \
		static uint32_t bb_path_id = 0;                                    \
		if(!bb_path_id) {                                                  \
			bb_resolve_path_id(__FILE__, &bb_path_id, (uint32_t)__LINE__); \
		}                                                                  \
		bb_thread_end(bb_path_id, (uint32_t)__LINE__);                     \
	}

#define _BB_LOG_INTERNAL(level, category, ...)                                                 \
	{                                                                                          \
		static uint32_t bb_path_id = 0;                                                        \
		static uint32_t bb_category_id = 0;                                                    \
		static uint32_t bb_id_resolved = 0;                                                    \
		if(!bb_id_resolved) {                                                                  \
			bb_id_resolved = _BB_RESOLVE_IDS_FUNC(__FILE__, category, &bb_path_id,             \
			                                      &bb_category_id, (uint32_t)__LINE__);        \
		}                                                                                      \
		_BB_TRACE_FUNC(bb_path_id, (uint32_t)__LINE__, bb_category_id, level, 0, __VA_ARGS__); \
	}

#define _BB_LOG_INTERNAL_A(level, category, ...)                                         \
	{                                                                                    \
		static uint32_t bb_path_id = 0;                                                  \
		static uint32_t bb_category_id = 0;                                              \
		static uint32_t bb_id_resolved = 0;                                              \
		if(!bb_id_resolved) {                                                            \
			bb_id_resolved = bb_resolve_ids(__FILE__, category, &bb_path_id,             \
			                                &bb_category_id, (uint32_t)__LINE__);        \
		}                                                                                \
		bb_trace(bb_path_id, (uint32_t)__LINE__, bb_category_id, level, 0, __VA_ARGS__); \
	}

#define BB_TRACE(logLevel, category, ...) _BB_LOG_INTERNAL(logLevel, category, __VA_ARGS__)
#define BB_LOG(category, ...) _BB_LOG_INTERNAL(kBBLogLevel_Log, category, __VA_ARGS__)
#define BB_WARNING(category, ...) _BB_LOG_INTERNAL(kBBLogLevel_Warning, category, __VA_ARGS__)
#define BB_ERROR(category, ...) _BB_LOG_INTERNAL(kBBLogLevel_Error, category, __VA_ARGS__)

#define BB_TRACE_A(logLevel, category, ...) _BB_LOG_INTERNAL_A(logLevel, category, __VA_ARGS__)
#define BB_LOG_A(category, ...) _BB_LOG_INTERNAL_A(kBBLogLevel_Log, category, __VA_ARGS__)
#define BB_WARNING_A(category, ...) _BB_LOG_INTERNAL_A(kBBLogLevel_Warning, category, __VA_ARGS__)
#define BB_ERROR_A(category, ...) _BB_LOG_INTERNAL_A(kBBLogLevel_Error, category, __VA_ARGS__)

#define BB_LOG_DYNAMIC(file, line, category, ...) _BB_TRACE_DYNAMIC_FUNC(file, line, category, kBBLogLevel_Log, 0, __VA_ARGS__)
#define BB_WARNING_DYNAMIC(file, line, category, ...) _BB_TRACE_DYNAMIC_FUNC(file, line, category, kBBLogLevel_Warning, 0, __VA_ARGS__)
#define BB_ERROR_DYNAMIC(file, line, category, ...) _BB_TRACE_DYNAMIC_FUNC(file, line, category, kBBLogLevel_Error, 0, __VA_ARGS__)

#define BB_LOG_DYNAMIC_PREFORMATTED(file, line, pieInstance, category, preformatted) _BB_TRACE_DYNAMIC_PREFORMATTED_FUNC(file, line, category, kBBLogLevel_Log, pieInstance, preformatted)
#define BB_WARNING_DYNAMIC_PREFORMATTED(file, line, pieInstance, category, preformatted) _BB_TRACE_DYNAMIC_PREFORMATTED_FUNC(file, line, category, kBBLogLevel_Warning, pieInstance, preformatted)
#define BB_ERROR_DYNAMIC_PREFORMATTED(file, line, pieInstance, category, preformatted) _BB_TRACE_DYNAMIC_PREFORMATTED_FUNC(file, line, category, kBBLogLevel_Error, pieInstance, preformatted)
#define BB_TRACE_DYNAMIC_PREFORMATTED(file, line, level, pieInstance, category, preformatted) _BB_TRACE_DYNAMIC_PREFORMATTED_FUNC(file, line, category, level, pieInstance, preformatted)

#define BB_TRACE_PARTIAL(logLevel, category, ...) _BB_TRACE_PARTIAL_FUNC(__FILE__, (uint32_t)__LINE__, category, logLevel, 0, __VA_ARGS__)
#define BB_LOG_PARTIAL(category, ...) _BB_TRACE_PARTIAL_FUNC(__FILE__, (uint32_t)__LINE__, category, kBBLogLevel_Log, 0, __VA_ARGS__)
#define BB_WARNING_PARTIAL(category, ...) _BB_TRACE_PARTIAL_FUNC(__FILE__, (uint32_t)__LINE__, category, kBBLogLevel_Warning, 0, __VA_ARGS__)
#define BB_ERROR_PARTIAL(category, ...) _BB_TRACE_PARTIAL_FUNC(__FILE__, (uint32_t)__LINE__, category, kBBLogLevel_Error, 0, __VA_ARGS__)

#define BB_LOG_PARTIAL_FROM_LOCATION(file, line, category, ...) _BB_TRACE_PARTIAL_FUNC(file, line, category, kBBLogLevel_Log, 0, __VA_ARGS__)
#define BB_WARNING_PARTIAL_FROM_LOCATION(file, line, category, ...) _BB_TRACE_PARTIAL_FUNC(file, line, category, kBBLogLevel_Warning, 0, __VA_ARGS__)
#define BB_ERROR_PARTIAL_FROM_LOCATION(file, line, category, ...) _BB_TRACE_PARTIAL_FUNC(file, line, category, kBBLogLevel_Error, 0, __VA_ARGS__)

#define BB_END_PARTIAL() bb_trace_partial_end()

typedef enum bb_color_e {
	kBBColor_Default,
	kBBColor_Evergreen_Black,
	kBBColor_Evergreen_Red,
	kBBColor_Evergreen_Green,
	kBBColor_Evergreen_Yellow,
	kBBColor_Evergreen_Blue,
	kBBColor_Evergreen_Cyan,
	kBBColor_Evergreen_Pink,
	kBBColor_Evergreen_White,
	kBBColor_Evergreen_LightBlue,
	kBBColor_Evergreen_Orange,
	kBBColor_Evergreen_LightBlueAlt,
	kBBColor_Evergreen_OrangeAlt,
	kBBColor_Evergreen_MediumBlue,
	kBBColor_Evergreen_Amber,
	kBBColor_UE4_Black,
	kBBColor_UE4_DarkRed,
	kBBColor_UE4_DarkGreen,
	kBBColor_UE4_DarkBlue,
	kBBColor_UE4_DarkYellow,
	kBBColor_UE4_DarkCyan,
	kBBColor_UE4_DarkPurple,
	kBBColor_UE4_DarkWhite,
	kBBColor_UE4_Red,
	kBBColor_UE4_Green,
	kBBColor_UE4_Blue,
	kBBColor_UE4_Yellow,
	kBBColor_UE4_Cyan,
	kBBColor_UE4_Purple,
	kBBColor_UE4_White,
	kBBColor_Count
} bb_color_t;

void bb_set_color(bb_color_t fg, bb_color_t bg);
#define BB_SET_COLOR(fg, bg) bb_set_color(fg, bg)

typedef struct bb_colors_s {
	bb_color_t fg;
	bb_color_t bg;
} bb_colors_t;

#else // #if BB_ENABLED

#define BB_INIT(applicationName)
#define BB_INIT_WITH_FLAGS(applicationName, flags)
#define BB_INIT_FROM_SOURCE(applicationName, sourceApplicationName, sourceIp)
#define BB_INIT_FROM_SOURCE_WITH_FLAGS(applicationName, sourceApplicationName, sourceIp, flags)
#define BB_SHUTDOWN()

#define BB_IS_CONNECTED() 0
#define BB_TICK()
#define BB_FLUSH()

#define BB_SET_INCOMING_CONSOLE_COMMAND_HANDLER(handler, context)

#define BB_THREAD_START(name)
#define BB_THREAD_SET_NAME(name)
#define BB_THREAD_END()

#define BB_TRACE(logLevel, category, ...)
#define BB_LOG(category, ...)
#define BB_WARNING(category, ...)
#define BB_ERROR(category, ...)

#define BB_LOG_DYNAMIC(file, line, category, ...)
#define BB_WARNING_DYNAMIC(file, line, category, ...)
#define BB_ERROR_DYNAMIC(file, line, category, ...)

#define BB_LOG_DYNAMIC_PREFORMATTED(file, line, category, preformatted)
#define BB_WARNING_DYNAMIC_PREFORMATTED(file, line, category, preformatted)
#define BB_ERROR_DYNAMIC_PREFORMATTED(file, line, category, preformatted)
#define BB_TRACE_DYNAMIC_PREFORMATTED(file, line, category, preformatted)

#define BB_LOG_PARTIAL(category, ...)
#define BB_WARNING_PARTIAL(category, ...)
#define BB_ERROR_PARTIAL(category, ...)

#define BB_LOG_PARTIAL_FROM_LOCATION(file, line, category, ...)
#define BB_WARNING_PARTIAL_FROM_LOCATION(file, line, category, ...)
#define BB_ERROR_PARTIAL_FROM_LOCATION(file, line, category, ...)

#define BB_END_PARTIAL()

#define BB_SET_COLOR(fg, bg)

#endif // #else // #if BB_ENABLED

#if defined(__cplusplus)
}
#endif

#endif // #ifndef BB_H
