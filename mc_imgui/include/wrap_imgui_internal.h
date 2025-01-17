// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "wrap_imgui.h"

#if BB_USING(BB_PLATFORM_WINDOWS)

// warning C4820 : 'StructName' : '4' bytes padding added after data member 'MemberName'
// warning C4365: '=': conversion from 'ImGuiTabItemFlags' to 'ImGuiID', signed/unsigned mismatch
// warning C5219: implicit conversion from 'int' to 'float', possible loss of data
BB_WARNING_PUSH(4820, 4365)
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
BB_WARNING_POP

#endif // #if BB_USING( BB_PLATFORM_WINDOWS )
