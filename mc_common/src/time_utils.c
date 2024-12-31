// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "time_utils.h"
#include "common.h"
#include "va.h"

#if BB_USING(BB_COMPILER_MSVC)

#include "bb_wrap_windows.h"

static u64 s_frameStartCounter;
static u64 s_counterFrequency;
static double s_counterToMilliseconds;

static double s_currentTime;
static float s_frameTimeFloat;
static double s_frameTimeDouble;
static u64 s_frameNumber;

double Time_GetCurrentFrameStartTime(void)
{
	return s_currentTime;
}

double Time_GetCurrentTime(void)
{
	if (!s_counterToMilliseconds)
	{
		LARGE_INTEGER li;
		QueryPerformanceFrequency(&li);
		s_counterFrequency = li.QuadPart;
		s_counterToMilliseconds = 1.0 / s_counterFrequency;
	}

	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart * s_counterToMilliseconds;
}

void Time_StartNewFrame(void)
{
	u64 oldFrameStartCounter = s_frameStartCounter;
	u64 deltaCounter;

	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	s_counterFrequency = li.QuadPart;
	s_counterToMilliseconds = 1.0 / s_counterFrequency;

	QueryPerformanceCounter(&li);
	s_frameStartCounter = li.QuadPart;

	deltaCounter = s_frameStartCounter - oldFrameStartCounter;
	s_frameTimeFloat = (float)(deltaCounter * s_counterToMilliseconds);
	s_frameTimeDouble = deltaCounter * s_counterToMilliseconds;
	s_currentTime = s_frameStartCounter * s_counterToMilliseconds;
	++s_frameNumber;
}

float Time_GetCurrentFrameElapsed(void)
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	u64 deltaCounter = li.QuadPart - s_frameStartCounter;
	return (float)(deltaCounter * s_counterToMilliseconds);
}

float Time_GetLastDT(void)
{
	return s_frameTimeFloat;
}

double Time_GetLastDTDouble(void)
{
	return s_frameTimeDouble;
}

u64 Time_GetFrameNumber(void)
{
	return s_frameNumber;
}

static SYSTEMTIME Time_SystemTimeFromEpochTime(u32 epochTime)
{
	// Windows uses 100 nanosecond intervals since Jan 1, 1601 UTC
	// https://support.microsoft.com/en-us/help/167296/how-to-convert-a-unix-time-t-to-a-win32-filetime-or-systemtime
	SYSTEMTIME st = { BB_EMPTY_INITIALIZER };
	FILETIME ft;
	u64 ll = (((u64)epochTime) * ((u64)10000000)) + (u64)116444736000000000;
	ft.dwLowDateTime = (DWORD)ll;
	ft.dwHighDateTime = ll >> 32;
	FileTimeToLocalFileTime(&ft, &ft);
	FileTimeToSystemTime(&ft, &st);
	return st;
}

const char* Time_StringFromEpochTime(u32 epochTime)
{
	SYSTEMTIME st = Time_SystemTimeFromEpochTime(epochTime);
	char dateBuffer[64] = "";
	char timeBuffer[64] = "";
	GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, dateBuffer, sizeof(dateBuffer));
	GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, timeBuffer, sizeof(timeBuffer));
	return va("%s %s", dateBuffer, timeBuffer);
}

#endif // #if BB_USING(BB_COMPILER_MSVC)
