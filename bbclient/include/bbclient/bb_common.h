// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#if defined(__cplusplus)
extern "C" {
#endif

#include "bb_assert.h"
#include "bb_defines.h"
#include "bb_types.h"

#if defined(__cplusplus)
}
#endif

#if BB_USING(BB_PLATFORM_WINDOWS) && _MSC_VER <= 1800 && !defined(__cplusplus)
#define BB_INLINE
#else
#define BB_INLINE inline
#endif

#if BB_USING(BB_PLATFORM_WINDOWS) && _MSC_VER <= 1800
#define bb_snprintf _snprintf
#else
#define bb_snprintf snprintf
#endif

#if BB_USING(BB_PLATFORM_WINDOWS)

// warning C4548: expression before comma has no effect; expected expression with side-effect
// warning C4820 : 'StructName' : '4' bytes padding added after data member 'MemberName'
// warning C4255: 'FuncName': no function prototype given: converting '()' to '(void)'
// warning C4668: '_WIN32_WINNT_WINTHRESHOLD' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
// warning C4574: 'INCL_WINSOCK_API_TYPEDEFS' is defined to be '0': did you mean to use '#if INCL_WINSOCK_API_TYPEDEFS'?
// warning C4365: 'return': conversion from 'b32' to 'BOOLEAN', signed/unsigned mismatch
// warning C4711 : function 'bbpacket_serialize_user' selected for automatic inline expansion
// warning C5045: Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
BB_WARNING_DISABLE(4711 5045)
BB_WARNING_PUSH(4820 4255 4668 4574 4365)
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h> // must come first before other windows headers

#include <ProcessThreadsApi.h>
#include <WS2tcpip.h>
#include <io.h>
#include <process.h>
#include <winsock2.h>
BB_WARNING_POP

#define BB_EMPTY_INITIALIZER 0

#else // #if BB_USING( BB_PLATFORM_WINDOWS )

#define BB_EMPTY_INITIALIZER

#endif // #else // #if BB_USING( BB_PLATFORM_WINDOWS )

#endif // #if BB_ENABLED
