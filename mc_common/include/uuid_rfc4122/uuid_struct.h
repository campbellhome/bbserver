#ifndef RFC_4122_UUID_STRUCT_H
#define RFC_4122_UUID_STRUCT_H

typedef unsigned long unsigned32;
typedef unsigned short unsigned16;
typedef unsigned char unsigned8;
typedef unsigned char byte;

typedef struct
{
	unsigned32 time_low;
	unsigned16 time_mid;
	unsigned16 time_hi_and_version;
	unsigned8 clock_seq_hi_and_reserved;
	unsigned8 clock_seq_low;
	byte node[6];
} rfc_uuid;

#endif // RFC_4122_UUID_STRUCT_H
