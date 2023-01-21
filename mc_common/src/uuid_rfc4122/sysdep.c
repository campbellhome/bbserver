#if _MSC_VER
__pragma(warning(disable : 4710)); // warning C4710: '__local_stdio_printf_options': function not inlined
__pragma(warning(disable : 4711)); // warning C4711: function 'Decode' selected for automatic inline expansion
__pragma(warning(disable : 4820)); // warning C4820: '_iobuf' : '4' bytes padding added after data member '_cnt'
#endif

#include "uuid_rfc4122/sysdep.h"
#include "uuid_rfc4122/copyrt.h"
#include <stdio.h>
#include <string.h>

/* system dependent call to get the current system time. Returned as
100ns ticks since UUID epoch, but resolution may be less than
100ns. */
#ifdef _WINDOWS_

void get_system_time(uuid_time_t* uuid_time)
{
	ULARGE_INTEGER time;

	/* NT keeps time in FILETIME format which is 100ns ticks since
	Jan 1, 1601. UUIDs use time in 100ns ticks since Oct 15, 1582.
	The difference is 17 Days in Oct + 30 (Nov) + 31 (Dec)
	+ 18 years and 5 leap days. */
	GetSystemTimeAsFileTime((FILETIME*)&time);
	time.QuadPart +=
	    (unsigned __int64)(1000 * 1000 * 10)               // seconds
	    * (unsigned __int64)(60 * 60 * 24)                 // days
	    * (unsigned __int64)(17 + 30 + 31 + 365 * 18 + 5); // # of days
	*uuid_time = time.QuadPart;
}

/* Sample code, not for use in production; see RFC 1750 */
void get_random_info(unsigned char seed[16])
{
	MD5_CTX c;
	struct
	{
		MEMORYSTATUSEX m;
		SYSTEM_INFO s;
		FILETIME t;
		LARGE_INTEGER pc;
		ULONGLONG tc;
		DWORD l;
		char hostname[MAX_COMPUTERNAME_LENGTH + 1];
	} r;

	MD5Init(&c);
	GlobalMemoryStatusEx(&r.m); // static analysis says GlobalMemoryStatus() is deprecated
	GetSystemInfo(&r.s);
	GetSystemTimeAsFileTime(&r.t);
	QueryPerformanceCounter(&r.pc);
	r.tc = GetTickCount64(); // static analysis warns on GetTickCount() usage
	r.l = MAX_COMPUTERNAME_LENGTH + 1;
	GetComputerNameA(r.hostname, &r.l);
	MD5Update(&c, &r, sizeof(r));
	MD5Final(seed, &c);
}

#else

#include <unistd.h>

void get_system_time(uuid_time_t* uuid_time)
{
	struct timeval tp;

	gettimeofday(&tp, (struct timezone*)0);

	/* Offset between UUID formatted times and Unix formatted times.
	UUID UTC base time is October 15, 1582.
	Unix base time is January 1, 1970.*/
	*uuid_time = ((u64)tp.tv_sec * 10000000) + ((u64)tp.tv_usec * 10) + I64(0x01B21DD213814000);
}

/* Sample code, not for use in production; see RFC 1750 */
void get_random_info(unsigned char seed[16])
{
	MD5_CTX c;
	struct
	{
		struct sysinfo s;
		struct timeval t;
		char hostname[257];
	} r;

	MD5Init(&c);
	sysinfo(&r.s);
	gettimeofday(&r.t, (struct timezone*)0);
	gethostname(r.hostname, 256);
	MD5Update(&c, &r, sizeof r);
	MD5Final(seed, &c);
}

static void fopen_s(FILE** fp, const char* filename, const char* mode)
{
	*fp = fopen(filename, mode);
}

#endif

/* system dependent call to get IEEE node ID.
This sample implementation generates a random node ID. */
uuid_node_t generate_uuid_node_identifier(void)
{
	uuid_node_t ret;
	unsigned char seed[16];
	get_random_info(seed);
	seed[0] |= 0x01;
	memcpy(&ret, seed, sizeof ret);
	return ret;
}

void get_ieee_node_identifier(uuid_node_t* node)
{
	static int inited = 0;
	static uuid_node_t saved_node;
	unsigned char seed[16];
	FILE* fp;

	if (!inited)
	{
		fopen_s(&fp, "nodeid", "rb");
		if (fp)
		{
			fread(&saved_node, sizeof saved_node, 1, fp);
			fclose(fp);
		}
		else
		{
			get_random_info(seed);
			seed[0] |= 0x01;
			memcpy(&saved_node, seed, sizeof saved_node);
			fopen_s(&fp, "nodeid", "wb");
			if (fp)
			{
				fwrite(&saved_node, sizeof saved_node, 1, fp);
				fclose(fp);
			}
		}
		inited = 1;
	}

	*node = saved_node;
}
