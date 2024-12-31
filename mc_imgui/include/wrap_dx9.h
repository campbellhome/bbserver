// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#ifndef WRAP_DX9_H
#define WRAP_DX9_H

#if _MSC_VER
__pragma(warning(push))
__pragma(warning(disable : 4820 4255 4668 4574 4365))
#include <d3d9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
__pragma(warning(pop))
#endif

#endif // #ifndef WRAP_DX9_H
