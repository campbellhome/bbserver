// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(_MSC_VER) && _MSC_VER
__pragma(warning(push))
__pragma(warning(disable : 4820))
#include <malloc.h>
__pragma(warning(pop))
#elif (defined(BB_FORCE_PLATFORM_ORBIS) && BB_FORCE_PLATFORM_ORBIS) || (defined(BB_FORCE_PLATFORM_PROSPERO) && BB_FORCE_PLATFORM_PROSPERO)
#include <stdlib.h>
#else
#include <malloc.h>
#endif
