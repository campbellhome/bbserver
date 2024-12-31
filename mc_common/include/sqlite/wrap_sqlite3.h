// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "bb_common.h"

#if BB_USING(BB_PLATFORM_WINDOWS)

// warning C4820 : 'StructName' : '4' bytes padding added after data member 'MemberName'
BB_WARNING_PUSH(4820)

#include "sqlite/sqlite3.h"

BB_WARNING_POP

#else

#include "sqlite/sqlite3.h"

#endif // #if BB_USING( BB_PLATFORM_WINDOWS )
