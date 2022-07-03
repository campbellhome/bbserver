#if _MSC_VER
__pragma(warning(disable : 4820)); // warning C4820: '_timespec64': '4' bytes padding added after data member 'tv_nsec' (compiling source file server\uuid_rfc4122\uuid.c)
__pragma(warning(disable : 4710)); // warning C4710: '__local_stdio_printf_options': function not inlined
__pragma(warning(disable : 4711)); // warning C4711: function 'Decode' selected for automatic inline expansion
#endif

#include "uuid_rfc4122/uuid.h"
#include "bb_criticalsection.h"
#include "bb_string.h"
#include "parson/parson.h"
#include "uuid_rfc4122/copyrt.h"
#include "uuid_rfc4122/sysdep.h"
#include "va.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_MSC_VER)
#include "bb_wrap_windows.h"
#include "bb_wrap_winsock2.h"
#else
#include <arpa/inet.h>
static void fopen_s(FILE **fp, const char *filename, const char *mode)
{
	*fp = fopen(filename, mode);
}
#endif

/* various forward declarations */
static int read_state(unsigned16 *clockseq, uuid_time_t *timestamp,
                      uuid_node_t *node);
static void write_state(unsigned16 clockseq, uuid_time_t timestamp,
                        uuid_node_t node);
static void format_uuid_v1(rfc_uuid *uuid, unsigned16 clockseq,
                           uuid_time_t timestamp, uuid_node_t node);
static void format_uuid_v3or5(rfc_uuid *uuid, unsigned char hash[16],
                              int v);
static void get_current_time(uuid_time_t *timestamp);
static unsigned16 true_random(void);

static bb_critical_section s_uuid_cs;
static uuid_state_read_func *s_state_read;
static uuid_state_write_func *s_state_write;

void uuid_init(uuid_state_read_func *state_read, uuid_state_write_func *state_write)
{
	s_state_read = state_read;
	s_state_write = state_write;
	bb_critical_section_init(&s_uuid_cs);
}

void uuid_shutdown(void)
{
	bb_critical_section_shutdown(&s_uuid_cs);
}

b32 uuid_is_initialized(void)
{
	return s_uuid_cs.initialized;
}

/* uuid_create -- generate a UUID */
int uuid_create(rfc_uuid *uuid)
{
	uuid_time_t timestamp, last_time;
	unsigned16 clockseq;
	uuid_node_t node;
	uuid_node_t last_node;
	int f;

	if(!s_uuid_cs.initialized)
		return 0;

	/* acquire system-wide lock so we're alone */
	bb_critical_section_lock(&s_uuid_cs);

	/* get time, node ID, saved state from non-volatile storage */
	get_current_time(&timestamp);

	if(s_state_read) {
		u32 timestampLow = 0;
		u32 timestampHigh = 0;
		(*s_state_read)(&clockseq, &timestampLow, &timestampHigh, &last_node);
		node = last_node;
		last_time = (((unsigned64_t)timestampHigh) << 32) | (timestampLow);
		f = last_time != 0;
	} else {
		get_ieee_node_identifier(&node);
		f = read_state(&clockseq, &last_time, &last_node);
	}

	/* if no NV state, or if clock went backwards, or node ID
	changed (e.g., new network card) change clockseq */
	if(!f || memcmp(&node, &last_node, sizeof node))
		clockseq = true_random();
	else if(timestamp < last_time)
		clockseq++;

	/* save the state for next time */
	write_state(clockseq, timestamp, node);

	bb_critical_section_unlock(&s_uuid_cs);

	/* stuff fields into the UUID */
	format_uuid_v1(uuid, clockseq, timestamp, node);
	return 1;
}

/* format_uuid_v1 -- make a UUID from the timestamp, clockseq,
and node ID */
void format_uuid_v1(rfc_uuid *uuid, unsigned16 clock_seq,
                    uuid_time_t timestamp, uuid_node_t node)
{
	/* Construct a version 1 uuid with the information we've gathered
	plus a few constants. */
	uuid->time_low = (unsigned long)(timestamp & 0xFFFFFFFF);
	uuid->time_mid = (unsigned short)((timestamp >> 32) & 0xFFFF);
	uuid->time_hi_and_version = (unsigned short)((timestamp >> 48) & 0x0FFF);
	uuid->time_hi_and_version |= (1 << 12);
	uuid->clock_seq_low = clock_seq & 0xFF;
	uuid->clock_seq_hi_and_reserved = (clock_seq & 0x3F00) >> 8;
	uuid->clock_seq_hi_and_reserved |= 0x80;
	memcpy(&uuid->node, &node, sizeof uuid->node);
}

/* data type for UUID generator persistent state */
typedef struct {
	uuid_time_t ts;   /* saved timestamp */
	uuid_node_t node; /* saved node ID */
	unsigned16 cs;    /* saved clock sequence */
} uuid_state;

static uuid_state st;

/* read_state -- read UUID generator state from non-volatile store */
static int read_state(unsigned16 *clockseq, uuid_time_t *timestamp,
                      uuid_node_t *node)
{
	static int inited = 0;
	FILE *fp;

	/* only need to read state once per boot */
	if(!inited) {
		fopen_s(&fp, "state", "rb");
		if(fp == NULL)
			return 0;
		fread(&st, sizeof st, 1, fp);
		fclose(fp);
		inited = 1;
	}
	*clockseq = st.cs;
	*timestamp = st.ts;
	*node = st.node;
	return 1;
}

/* write_state -- save UUID generator state back to non-volatile
storage */
static void write_state(unsigned16 clockseq, uuid_time_t timestamp,
                        uuid_node_t node)
{
	static int inited = 0;
	static uuid_time_t next_save;
	FILE *fp;
	if(!inited) {
		next_save = timestamp;
		inited = 1;
	}

	/* always save state to volatile shared state */
	st.cs = clockseq;
	st.ts = timestamp;
	st.node = node;
	if(timestamp >= next_save) {
		if(s_state_write) {
			(*s_state_write)(clockseq, (u32)timestamp, (u32)(timestamp >> 32), node);
		} else {
			fopen_s(&fp, "state", "wb");
			if(fp) {
				fwrite(&st, sizeof st, 1, fp);
				fclose(fp);
			}
		}
		/* schedule next save for 10 seconds from now */
		next_save = timestamp + (10 * 10 * 1000 * 1000);
	}
}

/* get-current_time -- get time as 60-bit 100ns ticks since UUID epoch.
Compensate for the fact that real clock resolution is
less than 100ns. */
void get_current_time(uuid_time_t *timestamp)
{
	static int inited = 0;
	static uuid_time_t time_last;
	static unsigned16 uuids_this_tick;
	uuid_time_t time_now;

	if(!inited) {
		get_system_time(&time_now);
		uuids_this_tick = UUIDS_PER_TICK;
		inited = 1;
	}

	for(;;) {
		get_system_time(&time_now);

		/* if clock reading changed since last UUID generated, */
		if(time_last != time_now) {
			/* reset count of uuids gen'd with this clock reading */
			uuids_this_tick = 0;
			time_last = time_now;
			break;
		}
		if(uuids_this_tick < UUIDS_PER_TICK) {
			uuids_this_tick++;
			break;
		}

		/* going too fast for our clock; spin */
	}
	/* add the count of uuids to low order bits of the clock reading */
	*timestamp = time_now + uuids_this_tick;
}

/* true_random -- generate a crypto-quality random number.
**This sample doesn't do that.** */
static unsigned16 true_random(void)
{
	static int inited = 0;
	uuid_time_t time_now;

	if(!inited) {
		get_system_time(&time_now);
		time_now = time_now / UUIDS_PER_TICK;
		srand((unsigned int)(((time_now >> 32) ^ time_now) & 0xffffffff));
		inited = 1;
	}

	return (unsigned16)rand();
}

/* uuid_create_md5_from_name -- create a version 3 (MD5) UUID using a
"name" from a "name space" */
void uuid_create_md5_from_name(rfc_uuid *uuid, rfc_uuid nsid, void *name,
                               int namelen)
{
	MD5_CTX c;
	unsigned char hash[16];
	rfc_uuid net_nsid;

	/* put name space ID in network byte order so it hashes the same
	no matter what endian machine we're on */
	net_nsid = nsid;
	net_nsid.time_low = htonl(net_nsid.time_low);
	net_nsid.time_mid = htons(net_nsid.time_mid);
	net_nsid.time_hi_and_version = htons(net_nsid.time_hi_and_version);

	MD5Init(&c);
	MD5Update(&c, &net_nsid, sizeof net_nsid);
	MD5Update(&c, name, namelen);
	MD5Final(hash, &c);

	/* the hash is in network byte order at this point */
	format_uuid_v3or5(uuid, hash, 3);
}

#ifdef USE_SHA1
void uuid_create_sha1_from_name(rfc_uuid *uuid, rfc_uuid nsid, void *name,
                                int namelen)
{
	SHA_CTX c;
	unsigned char hash[20];
	rfc_uuid net_nsid;

	/* put name space ID in network byte order so it hashes the same
	no matter what endian machine we're on */
	net_nsid = nsid;
	net_nsid.time_low = htonl(net_nsid.time_low);
	net_nsid.time_mid = htons(net_nsid.time_mid);
	net_nsid.time_hi_and_version = htons(net_nsid.time_hi_and_version);

	SHA1_Init(&c);
	SHA1_Update(&c, &net_nsid, sizeof net_nsid);
	SHA1_Update(&c, name, namelen);
	SHA1_Final(hash, &c);

	/* the hash is in network byte order at this point */
	format_uuid_v3or5(uuid, hash, 5);
}
#endif // USE_SHA1

/* format_uuid_v3or5 -- make a UUID from a (pseudo)random 128-bit
number */
void format_uuid_v3or5(rfc_uuid *uuid, unsigned char hash[16], int v)
{
	/* convert UUID to local byte order */
	memcpy(uuid, hash, sizeof *uuid);
	uuid->time_low = ntohl(uuid->time_low);
	uuid->time_mid = ntohs(uuid->time_mid);
	uuid->time_hi_and_version = ntohs(uuid->time_hi_and_version);

	/* put in the variant and version bits */
	uuid->time_hi_and_version &= 0x0FFF;
	uuid->time_hi_and_version |= (v << 12);
	uuid->clock_seq_hi_and_reserved &= 0x3F;
	uuid->clock_seq_hi_and_reserved |= 0x80;
}

/* uuid_compare --  Compare two UUID's "lexically" and return */
#define CHECK(f1, f2) \
	if(f1 != f2)      \
		return f1 < f2 ? -1 : 1;
int uuid_compare(rfc_uuid *u1, rfc_uuid *u2)
{
	int i;

	CHECK(u1->time_low, u2->time_low);
	CHECK(u1->time_mid, u2->time_mid);
	CHECK(u1->time_hi_and_version, u2->time_hi_and_version);
	CHECK(u1->clock_seq_hi_and_reserved, u2->clock_seq_hi_and_reserved);
	CHECK(u1->clock_seq_low, u2->clock_seq_low)
	for(i = 0; i < 6; i++) {
		if(u1->node[i] < u2->node[i])
			return -1;
		if(u1->node[i] > u2->node[i])
			return 1;
	}
	return 0;
}
#undef CHECK

int format_uuid(rfc_uuid *uuid, char *buffer, int bufferSize)
{
	int len = bb_snprintf(buffer, bufferSize, "%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x", (unsigned)uuid->time_low, uuid->time_mid,
	                      uuid->time_hi_and_version, uuid->clock_seq_hi_and_reserved,
	                      uuid->clock_seq_low,
	                      uuid->node[0], uuid->node[1], uuid->node[2],
	                      uuid->node[3], uuid->node[4], uuid->node[5]);
	if(len < 0) {
		len = bufferSize - 1;
		buffer[len] = '\0';
	}
	return len;
}

void uuid_node_reset(uuid_node_t *val)
{
	if(val) {
	}
}

uuid_node_t uuid_node_clone(const uuid_node_t *src)
{
	uuid_node_t dst = { BB_EMPTY_INITIALIZER };
	if(src) {
		for(u32 i = 0; i < BB_ARRAYSIZE(src->nodeID); ++i) {
			dst.nodeID[i] = src->nodeID[i];
		}
	}
	return dst;
}

uuid_node_t json_deserialize_uuid_node_t(JSON_Value *src)
{
	uuid_node_t dst;
	memset(&dst, 0, sizeof(dst));
	if(src) {
		JSON_Object *obj = json_value_get_object(src);
		if(obj) {
			for(u32 i = 0; i < BB_ARRAYSIZE(dst.nodeID); ++i) {
				dst.nodeID[i] = (char)json_object_get_number(obj, va("nodeID.%u", i));
			}
		}
	}
	return dst;
}

JSON_Value *json_serialize_uuid_node_t(const uuid_node_t *src)
{
	JSON_Value *val = json_value_init_object();
	JSON_Object *obj = json_value_get_object(val);
	if(obj) {
		for(u32 i = 0; i < BB_ARRAYSIZE(src->nodeID); ++i) {
			json_object_set_number(obj, va("nodeID.%u", i), src->nodeID[i]);
		}
	}
	return val;
}
