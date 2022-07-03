// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"

#include "bbclient/bb_time.h"

#if BB_USING(BB_COMPILER_MSVC)

#include "bbclient/bb_wrap_windows.h"

u64 bb_current_ticks(void)
{
	LARGE_INTEGER li;
	li.QuadPart = 0;
	QueryPerformanceCounter(&li);
	return (u64)li.QuadPart;
}

double bb_millis_per_tick(void)
{
	LARGE_INTEGER li;
	li.QuadPart = 1;
	QueryPerformanceFrequency(&li);
	return 1000.0 / li.QuadPart;
}

u64 bb_current_time_ms(void)
{
	return GetTickCount64();
}

u64 bb_current_time_microseconds_from_epoch(void)
{
	FILETIME ft;
	SYSTEMTIME st;
	ULARGE_INTEGER uli;
	const u64 epoch = 116444736000000000Ui64;

	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;

	return st.wMilliseconds * (u64)1000 + (uli.QuadPart - epoch) / 10;
}

void bb_sleep_ms(u32 millis)
{
	Sleep(millis);
}

#endif // #if BB_USING(BB_COMPILER_MSVC)

#if BB_USING(BB_COMPILER_CLANG)

#include <time.h>
#include <unistd.h>

u64 bb_current_ticks(void)
{
	return bb_current_time_ms();
}

double bb_millis_per_tick(void)
{
	return 1.0;
}

u64 bb_current_time_ms(void)
{
	struct timespec ts;
	if(clock_gettime(CLOCK_REALTIME, &ts) == 0) {
		return (ts.tv_sec) * 1000ULL + (ts.tv_nsec) / 1000000ULL;
	}
	return 0;
}

u64 bb_current_time_microseconds_from_epoch(void)
{
	struct timespec ts;
	if(clock_gettime(CLOCK_REALTIME, &ts) == 0) {
		return (ts.tv_sec) * 1000000ULL + (ts.tv_nsec) / 1000ULL;
	}
	return 0;
}

void bb_sleep_ms(u32 millis)
{
	usleep(millis * 1000);
}

#endif // #if BB_USING(BB_COMPILER_CLANG)

#endif // #if BB_ENABLED
