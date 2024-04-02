// Copyright (c) 2012-2024 Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"

#if defined(_MSC_VER)
__pragma(warning(disable : 4710)) // warning C4710 : 'int printf(const char *const ,...)' : function not inlined
#endif

#include "bbclient/bb_log.h"
#include "bbclient/bb_sockets.h"
#include "bbclient/bb_time.h"
#include "bbclient/bb_wrap_stdio.h"
#include "bbclient/bb_wrap_windows.h"
#include <string.h>

extern u32 g_bb_initFlags;

static u64 s_log_start_ms;

void bb_log_init(void)
{
	s_log_start_ms = bb_current_time_ms();
}

void bb_log_shutdown(void)
{
}

static void bb_vlog(const char* fmt, va_list args)
{
	int len;
	char szBuffer[512];
	int prefixLen;

	if ((g_bb_initFlags & kBBInitFlag_DebugInit) == 0)
		return;

	prefixLen = bb_snprintf(szBuffer, sizeof(szBuffer), "%" PRIu64 " ms: ", bb_current_time_ms() - s_log_start_ms);
	if (prefixLen < 0)
	{
		prefixLen = 0;
	}

	len = vsnprintf(szBuffer + prefixLen, sizeof(szBuffer) - (u64)prefixLen, fmt, args);
	if (len == 0)
		return;

	if (len < 0)
	{
		len = sizeof(szBuffer) - 2;
	}
	else
	{
		len = BB_MIN(len + prefixLen, (int)sizeof(szBuffer) - 2);
	}

	if (szBuffer[len - 1] != '\n')
	{
		szBuffer[len++] = '\n';
	}
	szBuffer[len] = '\0';

#if BB_USING(BB_PLATFORM_WINDOWS)
	OutputDebugStringA(szBuffer);
#endif // #if BB_USING( BB_PLATFORM_WINDOWS )
	fwrite(szBuffer, (size_t)len, 1, stdout);
}

void bb_log(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	bb_vlog(fmt, args);
	va_end(args);
}

void bb_warning(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	bb_vlog(fmt, args);
	va_end(args);
}

void bb_error(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	bb_vlog(fmt, args);
	va_end(args);
}

BB_WARNING_DISABLE(4710) // snprintf not inlined - can't push/pop because it happens later

char* bb_format_ipport(char* buf, size_t len, u32 addr, u16 port)
{
	int ret = bb_snprintf(buf, len, "%u.%u.%u.%u:%u",
	                      (addr & 0xFF000000) >> 24,
	                      (addr & 0x00FF0000) >> 16,
	                      (addr & 0x0000FF00) >> 8,
	                      (addr & 0x000000FF),
	                      port);
	ret = (ret > 0 && ret < (int)len) ? ret : (int)len - 1;
	buf[ret] = '\0';
	return buf;
}

char* bb_format_ip(char* buf, size_t len, u32 addr)
{
	int ret = bb_snprintf(buf, len, "%u.%u.%u.%u",
	                      (addr & 0xFF000000) >> 24,
	                      (addr & 0x00FF0000) >> 16,
	                      (addr & 0x0000FF00) >> 8,
	                      (addr & 0x000000FF));
	ret = (ret > 0 && ret < (int)len) ? ret : (int)len - 1;
	buf[ret] = '\0';
	return buf;
}

char* bb_format_addr(char* buf, size_t bufLen, const struct sockaddr* addr, size_t addrSize, b32 addPort)
{
	if (!buf || !bufLen || !addr || !addrSize)
		return "";

	if (!addr || !addrSize)
	{
		*buf = '\0';
		return buf;
	}

	char* out = buf;

	if (addr->sa_family == AF_INET6 && addPort)
	{
		*buf = '[';
		++buf;
		--bufLen;
	}

	getnameinfo(addr, (socklen_t)addrSize, buf, (socklen_t)bufLen, NULL, 0, NI_NUMERICHOST);
	size_t used = strlen(buf);
	buf += used;
	bufLen -= used;

	if (addPort)
	{
		if (addr->sa_family == AF_INET6 && bufLen)
		{
			*buf = ']';
			++buf;
			--bufLen;
		}

		u16 port = ntohs((addr->sa_family == AF_INET6) ? ((const struct sockaddr_in6*)addr)->sin6_port : ((const struct sockaddr_in*)addr)->sin_port);
		if (bufLen)
		{
			bb_snprintf(buf, bufLen, ":%u", port);
		}
	}

	return out;
}

#endif // #if BB_ENABLED
