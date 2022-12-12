// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "bb_common.h"

#if BB_USING(BB_PLATFORM_WINDOWS)

#include "imgui_core_freetype.h"

// can't push/pop because these happen after initial compilation phase:
BB_WARNING_DISABLE(4514) // unreferenced inline function has been removed
BB_WARNING_DISABLE(4710) // snprintf not inlined

// warning C4548: expression before comma has no effect; expected expression with side-effect
// warning C4820 : 'StructName' : '4' bytes padding added after data member 'MemberName'
// warning C4255: 'FuncName': no function prototype given: converting '()' to '(void)'
// warning C4668: '_WIN32_WINNT_WINTHRESHOLD' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
// warning C4574: 'INCL_WINSOCK_API_TYPEDEFS' is defined to be '0': did you mean to use '#if INCL_WINSOCK_API_TYPEDEFS'?
// warning C4365: 'return': conversion from 'bool' to 'BOOLEAN', signed/unsigned mismatch
// warning C4459: declaration of 'var' hides global declaration
BB_WARNING_PUSH(4820 4255 4668 4574 4365 4459)

#include "imgui.h"

#include "backends/imgui_impl_dx9.h"
#include "backends/imgui_impl_win32.h"
#include "forkawesome-webfont.h"
#if BB_USING(FEATURE_FREETYPE)
#include "misc/freetype/imgui_freetype.h"
#endif // #if BB_USING(FEATURE_FREETYPE)

//#include <tchar.h>

BB_WARNING_POP

#endif // #if BB_USING( BB_PLATFORM_WINDOWS )
