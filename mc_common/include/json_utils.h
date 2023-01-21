// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct json_object_t JSON_Object;
int json_object_get_boolean_safe(const JSON_Object* object, const char* name);

#if defined(__cplusplus)
}
#endif
