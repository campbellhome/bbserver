// Copyright (c) 2012-2024 Matt Campbell
// MIT license (see License.txt)

#include "tags.h"
#include "appdata.h"
#include "bb_array.h"
#include "bb_json_generated.h"
#include "bb_structs_generated.h"
#include "bb_wrap_stdio.h"
#include "recorded_session.h"
#include "view.h"
#include <stdlib.h>
#include <string.h>

tagData_t g_tags;

static sb_t tags_get_path(void)
{
	sb_t s = appdata_get("bb");
	sb_append(&s, "\\bb_tags_config.json");
	return s;
}

void tags_init(void)
{
	bba_add(g_tags.tagTable, 256);
	bba_add(g_tags.categoryTable, 256);

	sb_t path = tags_get_path();
	JSON_Value* val = json_parse_file(sb_get(&path));
	if (val)
	{
		tags_config_t tagsConfig = json_deserialize_tags_config_t(val);
		json_value_free(val);

		for (u32 i = 0; i < tagsConfig.tags.count; ++i)
		{
			tag_t* configTag = tagsConfig.tags.data + i;
			for (u32 j = 0; j < configTag->categories.count; ++j)
			{
				sb_t* category = configTag->categories.data + j;
				tag_add_category(sb_get(&configTag->name), sb_get(category));
			}
			tag_t* tag = tag_find(sb_get(&configTag->name));
			if (tag)
			{
				tag->visibility = configTag->visibility;
				tag->noColor = configTag->noColor;
			}
		}

		tags_config_reset(&tagsConfig);
	}
	sb_reset(&path);
}

void tags_write(void)
{
	tags_config_t tagsConfig = { BB_EMPTY_INITIALIZER };
	tagsConfig.tags = g_tags.tags;

	JSON_Value* val = json_serialize_tags_config_t(&tagsConfig);
	if (val)
	{
		sb_t path = tags_get_path();
		FILE* fp = fopen(sb_get(&path), "wb");
		if (fp)
		{
			char* serialized_string = json_serialize_to_string_pretty(val);
			fputs(serialized_string, fp);
			fclose(fp);
			json_free_serialized_string(serialized_string);
		}
		sb_reset(&path);
	}
	json_value_free(val);
}

void tags_shutdown(void)
{
	tagData_reset(&g_tags);
}

static void tag_add_entry(sbsHashTable* table, const char* key, const char* value)
{
	sbsHashEntry* entry = sbsHashTable_find(table, key);
	if (!entry)
	{
		sbsHashEntry newEntry = { BB_EMPTY_INITIALIZER };
		newEntry.key = sb_from_c_string_no_alloc(key);
		entry = sbsHashTable_insert(table, &newEntry);
	}
	if (!entry)
	{
		return;
	}
	for (u32 i = 0; i < entry->values.count; ++i)
	{
		sb_t* entryValue = entry->values.data + i;
		if (!strcmp(sb_get(entryValue), value))
		{
			return;
		}
	}
	bba_push(entry->values, sb_from_c_string(value));
}

static void tag_remove_entry(sbsHashTable* table, const char* key, const char* value)
{
	sbsHashEntry* entry = sbsHashTable_find(table, key);
	if (!entry)
	{
		return;
	}
	for (u32 i = 0; i < entry->values.count; ++i)
	{
		sb_t* entryValue = entry->values.data + i;
		if (!strcmp(sb_get(entryValue), value))
		{
			sb_reset(entryValue);
			bba_erase(entry->values, i);
			break;
		}
	}
}

tag_t* tag_find(const char* tagName)
{
	for (u32 i = 0; i < g_tags.tags.count; ++i)
	{
		tag_t* tag = g_tags.tags.data + i;
		if (!strcmp(sb_get(&tag->name), tagName))
		{
			return tag;
		}
	}
	return NULL;
}

void tag_remove(tag_t* tag)
{
	for (u32 i = 0; i < tag->categories.count; ++i)
	{
		const char* categoryName = sb_get(tag->categories.data + i);
		sbsHashTable_remove(&g_tags.categoryTable, categoryName);
		tagCategory_t* category = tagCategory_find(categoryName);
		if (category)
		{
			u32 categoryIndex = (u32)(category - g_tags.categories.data);
			tagCategory_reset(g_tags.categories.data + categoryIndex);
			bba_erase(g_tags.categories, categoryIndex);
		}
	}

	sbsHashTable_remove(&g_tags.tagTable, sb_get(&tag->name));

	u32 tagIndex = (u32)(tag - g_tags.tags.data);
	tag_reset(tag);
	bba_erase(g_tags.tags, tagIndex);
}

static int tag_sort(const void* _a, const void* _b)
{
	const tag_t* a = (const tag_t*)_a;
	const tag_t* b = (const tag_t*)_b;
	return strcmp(sb_get(&a->name), sb_get(&b->name));
}

tag_t* tag_find_or_add(const char* tagName)
{
	tag_t* tag = tag_find(tagName);
	if (!tag)
	{
		tag = bba_add(g_tags.tags, 1);
		if (tag)
		{
			tag->name = sb_from_c_string(tagName);
		}
		qsort(g_tags.tags.data, g_tags.tags.count, sizeof(g_tags.tags.data[0]), tag_sort);
		tag = tag_find(tagName);
	}
	return tag;
}

tagCategory_t* tagCategory_find(const char* categoryName)
{
	for (u32 i = 0; i < g_tags.categories.count; ++i)
	{
		tagCategory_t* category = g_tags.categories.data + i;
		if (!strcmp(sb_get(&category->name), categoryName))
		{
			return category;
		}
	}
	return NULL;
}

static int tagCategory_sort(const void* _a, const void* _b)
{
	const tagCategory_t* a = (const tagCategory_t*)_a;
	const tagCategory_t* b = (const tagCategory_t*)_b;
	return strcmp(sb_get(&a->name), sb_get(&b->name));
}

tagCategory_t* tagCategory_find_or_add(const char* categoryName)
{
	tagCategory_t* category = tagCategory_find(categoryName);
	if (!category)
	{
		category = bba_add(g_tags.categories, 1);
		if (category)
		{
			category->name = sb_from_c_string(categoryName);
		}
		qsort(g_tags.categories.data, g_tags.categories.count, sizeof(g_tags.categories.data[0]), tagCategory_sort);
		category = tagCategory_find(categoryName);
	}
	return category;
}

void tag_add_category(const char* tagName, const char* categoryName)
{
	tag_add_entry(&g_tags.tagTable, tagName, categoryName);
	tag_add_entry(&g_tags.categoryTable, categoryName, tagName);

	tag_t* tag = tag_find_or_add(tagName);
	if (tag)
	{
		sbs_add_unique(&tag->categories, sb_from_c_string_no_alloc(categoryName));
		qsort(tag->categories.data, tag->categories.count, sizeof(tag->categories.data[0]), sbs_sort);
	}

	tagCategory_t* tagCategory = tagCategory_find_or_add(categoryName);
	if (tagCategory)
	{
		sbs_add_unique(&tagCategory->tags, sb_from_c_string_no_alloc(tagName));
		qsort(tagCategory->tags.data, tagCategory->tags.count, sizeof(tagCategory->tags.data[0]), sbs_sort);
	}
}

void tag_remove_category(const char* tagName, const char* categoryName)
{
	tag_remove_entry(&g_tags.tagTable, tagName, categoryName);
	tag_remove_entry(&g_tags.categoryTable, categoryName, tagName);

	tag_t* tag = tag_find(tagName);
	if (tag)
	{
		sbs_remove_unique(&tag->categories, sb_from_c_string_no_alloc(categoryName));
	}

	tagCategory_t* tagCategory = tagCategory_find(categoryName);
	if (tagCategory)
	{
		sbs_remove_unique(&tagCategory->tags, sb_from_c_string_no_alloc(tagName));
	}
}

void tag_apply_tag_to_all_views(void)
{
	for (u32 sessionIndex = 0; sessionIndex < recorded_session_count(); ++sessionIndex)
	{
		recorded_session_t* session = recorded_session_get(sessionIndex);
		if (!session || session->appInfo.type == kBBPacketType_Invalid)
			continue;

		for (u32 viewIndex = 0; viewIndex < session->views.count; ++viewIndex)
		{
			view_t* sessionView = session->views.data + viewIndex;
			view_apply_tag(sessionView);
			sessionView->visibleLogsDirty = true;
		}
	}
}
