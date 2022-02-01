#ifndef RFC_4122_UUID_H
#define RFC_4122_UUID_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "copyrt.h"

#include "uuid_struct.h"

#include "bb_types.h"
typedef struct uuid_node_s uuid_node_t;
typedef void(uuid_state_read_func)(u16 *clockSequence, u32 *timestampLow, u32 *timestampHi, uuid_node_t *nodeId);
typedef void(uuid_state_write_func)(u16 clockSequence, u32 timestampLow, u32 timestampHi, uuid_node_t nodeId);

void uuid_init(uuid_state_read_func *state_read, uuid_state_write_func *state_write);
void uuid_shutdown(void);
b32 uuid_is_initialized(void);

/* uuid_create -- generate a UUID */
int uuid_create(rfc_uuid *uuid);

/* uuid_create_md5_from_name -- create a version 3 (MD5) UUID using a
	"name" from a "name space" */
void uuid_create_md5_from_name(
    rfc_uuid *uuid, /* resulting UUID */
    rfc_uuid nsid,  /* UUID of the namespace */
    void *name,     /* the name from which to generate a UUID */
    int namelen     /* the length of the name */
    );

#ifdef USE_SHA1
/* uuid_create_sha1_from_name -- create a version 5 (SHA-1) UUID
	using a "name" from a "name space" */
void uuid_create_sha1_from_name(

    rfc_uuid *uuid, /* resulting UUID */
    rfc_uuid nsid,  /* UUID of the namespace */
    void *name,     /* the name from which to generate a UUID */
    int namelen     /* the length of the name */
    );
#endif // USE_SHA1

/* uuid_compare --  Compare two UUID's "lexically" and return
		-1   u1 is lexically before u2
			0   u1 is equal to u2
			1   u1 is lexically after u2
	Note that lexical ordering is not temporal ordering!
*/
int uuid_compare(rfc_uuid *u1, rfc_uuid *u2);

int format_uuid(rfc_uuid *uuid, char *buffer, int bufferSize);

#if defined(__cplusplus)
}
#endif

#endif // RFC_4122_UUID_H
