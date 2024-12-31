// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(_MSC_VER) && _MSC_VER

// warning C4548: expression before comma has no effect; expected expression with side-effect
// warning C4820 : 'StructName' : '4' bytes padding added after data member 'MemberName'
// warning C4255: 'FuncName': no function prototype given: converting '()' to '(void)'
// warning C4668: '_WIN32_WINNT_WINTHRESHOLD' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
// warning C4574: 'INCL_WINSOCK_API_TYPEDEFS' is defined to be '0': did you mean to use '#if INCL_WINSOCK_API_TYPEDEFS'?
// warning C4365: 'return': conversion from 'b32' to 'BOOLEAN', signed/unsigned mismatch
// warning C4711 : function 'bbpacket_serialize_user' selected for automatic inline expansion
// warning C5045: Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
__pragma(warning(push))
__pragma(warning(disable : 4711 5045 4820 4255 4668 4574 4365))
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h> // must come first before other windows headers
__pragma(warning(pop))

#endif
