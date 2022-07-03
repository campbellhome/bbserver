#pragma once

#include "common.h"
#include "copyrt.h"

/* remove the following define if you aren't running WIN32 */
#if BB_USING(BB_PLATFORM_WINDOWS)
#define WININC
#endif

#ifdef WININC
__pragma(warning(disable : 4820 4255 4668 4574));
#include "bb_wrap_windows.h"
#else
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

#include "md5_rfc1321/global.h"
#include "md5_rfc1321/md5.h"

/* set the following to the number of 100ns ticks of the actual
resolution of your system's clock */
#define UUIDS_PER_TICK 1024

/*
typedef unsigned long   unsigned32;
typedef unsigned short  unsigned16;
typedef unsigned char   unsigned8;
typedef unsigned char   byte;
*/

/* Set this to what your compiler uses for 64-bit data type */
#ifdef WININC
#define unsigned64_t unsigned __int64
#define I64(C) C
#else
#define unsigned64_t unsigned long long
#define I64(C) C##LL
#endif

#ifndef AUTOJSON
#define AUTOJSON
#define AUTOHEADERONLY
#endif

typedef unsigned64_t uuid_time_t;
AUTOJSON AUTOHEADERONLY typedef struct uuid_node_s {
	char nodeID[6];
} uuid_node_t;

uuid_node_t generate_uuid_node_identifier(void);
void get_ieee_node_identifier(uuid_node_t *node);
void get_system_time(uuid_time_t *uuid_time);
