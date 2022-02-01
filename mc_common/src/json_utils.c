// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "json_utils.h"
#include "parson/parson.h"

int json_object_get_boolean_safe(const JSON_Object *object, const char *name)
{
	return json_value_get_boolean(json_object_get_value(object, name)) == 1;
}
