// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb_wrap_windows.h"

#if defined(_MSC_VER) && _MSC_VER
__pragma(warning(push))
__pragma(warning(disable : 4464 4820 4255))
#include "dirent/include/dirent.h"
__pragma(warning(pop))
#else
#include <dirent.h>
#endif
