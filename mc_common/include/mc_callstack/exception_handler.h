// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _EXCEPTION_POINTERS EXCEPTION_POINTERS;

typedef void(exception_handler_callback)(EXCEPTION_POINTERS *pExPtrs);
void install_unhandled_exception_handler(exception_handler_callback *callback);

#if defined(__cplusplus)
}
#endif
