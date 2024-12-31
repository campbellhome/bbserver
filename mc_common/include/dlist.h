// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#define DLIST_INIT(sentinel)       \
	(sentinel)->next = (sentinel); \
	(sentinel)->prev = (sentinel);

#define DLIST_REMOVE(member)               \
	(member)->next->prev = (member)->prev; \
	(member)->prev->next = (member)->next; \
	(member)->next = (member)->prev = (member);

#define DLIST_INSERT_AFTER(other, member) \
	(member)->next = (other)->next;       \
	(member)->prev = (other);             \
	(other)->next->prev = (member);       \
	(other)->next = (member);
