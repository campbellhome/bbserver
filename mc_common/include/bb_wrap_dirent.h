// Copyright (c) 2012-2023 Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(_MSC_VER) && _MSC_VER
__pragma(warning(push))
__pragma(warning(disable : 4464 4820 4255))
#include "dirent/include/dirent.h"
__pragma(warning(pop))
#else
#include <dirent.h>
#endif
