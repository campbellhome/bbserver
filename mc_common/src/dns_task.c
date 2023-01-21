// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "dns_task.h"
#include "bb_array.h"
#include "bb_malloc.h"
#include "bb_sockets.h"
#include "bb_thread.h"
#include "va.h"

typedef struct tag_dns_task_userdata
{
	bb_thread_handle_t threadHandle;
	volatile b32 threadExitRequest;
	volatile taskState threadExitState;
	dns_task_result result;
	DnsTask_Finished* finishedFunc;
} dns_task_userdata;

static bb_thread_return_t dns_task_thread_proc(void* args)
{
	dns_task_userdata* userdata = args;

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // use AF_INET6 to force IPv6, AF_UNSPEC to get both
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // for use in bind(), gets local address

	struct addrinfo* addrinfos;
	int rv = getaddrinfo(sb_get(&userdata->result.name), "1492", &hints, &addrinfos);
	if (rv != 0)
	{
		BB_ERROR("dns_task", "getaddrinfo %s: %s", sb_get(&userdata->result.name), gai_strerror(rv));
		userdata->threadExitState = kTaskState_Failed;
		return 0;
	}

	struct addrinfo* p;
	struct sockaddr_in* addr;
	for (p = addrinfos; p != NULL; p = p->ai_next)
	{
		if (p->ai_addr->sa_family != AF_INET)
			continue;
		addr = (struct sockaddr_in*)(p->ai_addr);
		bba_push(userdata->result.addrs, ntohl(BB_S_ADDR_UNION(*addr)));
	}

	freeaddrinfo(addrinfos);
	userdata->threadExitState = kTaskState_Succeeded;
	return 0;
}

static void dns_task_tick(task* t)
{
	dns_task_userdata* userdata = (dns_task_userdata*)t->taskData;
	if (userdata && userdata->threadHandle)
	{
		if (userdata->threadExitState != kTaskState_Running)
		{
			task_set_state(t, userdata->threadExitState);
		}
	}
	else
	{
		task_set_state(t, kTaskState_Failed);
	}
	if (t->subtasks.count)
	{
		task_tick_subtasks(t);
	}
}

static void dns_task_statechanged(task* t)
{
	if (t->state == kTaskState_Running)
	{
		dns_task_userdata* userdata = (dns_task_userdata*)t->taskData;
		if (userdata)
		{
			userdata->threadHandle = bbthread_create(dns_task_thread_proc, userdata);
			if (userdata->threadHandle)
			{
				userdata->threadExitState = kTaskState_Running;
			}
		}
	}
	else if (task_done(t))
	{
		dns_task_userdata* userdata = (dns_task_userdata*)t->taskData;
		if (userdata && userdata->finishedFunc)
		{
			(*userdata->finishedFunc)(t, &userdata->result);
		}
	}
}

void dns_task_reset(task* t)
{
	dns_task_userdata* userdata = (dns_task_userdata*)t->taskData;
	if (userdata)
	{
		if (userdata->threadHandle && userdata->threadExitState == kTaskState_Running)
		{
			userdata->threadExitRequest = true;
			bbthread_join(userdata->threadHandle);
		}
		sb_reset(&userdata->result.name);
		bba_free(userdata->result.addrs);
		bb_free(t->taskData);
		t->taskData = NULL;
	}
}

task dns_task_create(const char* name, DnsTask_Finished* finished)
{
	task t = { BB_EMPTY_INITIALIZER };
	sb_append(&t.name, name);
	t.tick = dns_task_tick;
	t.stateChanged = dns_task_statechanged;
	t.reset = dns_task_reset;
	t.taskData = bb_malloc(sizeof(dns_task_userdata));
	if (t.taskData)
	{
		dns_task_userdata* userdata = (dns_task_userdata*)t.taskData;
		memset(userdata, 0, sizeof(*userdata));
		sb_append(&userdata->result.name, name);
		userdata->finishedFunc = finished;
	}
	return t;
}

dns_task_result* dns_task_get_result(task* t)
{
	if (!t || t->tick != dns_task_tick)
		return NULL;

	dns_task_userdata* userdata = (dns_task_userdata*)t->taskData;
	if (!userdata)
	{
		return NULL;
	}

	return &userdata->result;
}
