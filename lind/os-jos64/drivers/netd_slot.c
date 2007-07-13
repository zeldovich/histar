#include <stdint.h>
#include <inc/jthread.h>
#include <inc/assert.h>
#include <string.h>

#include "netduser.h"

struct sock_slot slots[16];
jthread_mutex_t slots_mu;

struct sock_slot *
slot_alloc(void)
{
    int i;
    jthread_mutex_lock(&slots_mu);
    for(i = 0; i < sizeof(slots) / sizeof(struct sock_slot); i++) {
	if (!slots[i].used) {
	    memset(&slots[i], 0, sizeof(slots[i]));
	    slots[i].sock = -1;
	    slots[i].used = 1;
	    jthread_mutex_unlock(&slots_mu);
	    return &slots[i];
	}
    }
    jthread_mutex_unlock(&slots_mu);
    return 0;
}

int
slot_to_id(struct sock_slot *ss)
{
    return ((uintptr_t)ss - (uintptr_t)&slots[0]) / sizeof(struct sock_slot);
}

struct sock_slot *
slot_from_id(int id)
{
    assert(id < sizeof(slots) / sizeof(struct sock_slot));
    return &slots[id];
}

void
slot_free(struct sock_slot *ss)
{
    jthread_mutex_lock(&slots_mu);
    ss->used = 0;
    jthread_mutex_unlock(&slots_mu);
}

void
slot_for_each(void (*op)(struct sock_slot*, void*), void *arg)
{
    int i;
    jthread_mutex_lock(&slots_mu);
    for(i = 0; i < sizeof(slots) / sizeof(struct sock_slot); i++)
	if (slots[i].used)
	    op(&slots[i], arg);
    jthread_mutex_unlock(&slots_mu);
}

static void
init_slot(struct sock_slot *s, void *x)
{
    s->sock = -1;
}

void
slot_init(void)
{
    memset(slots, 0, sizeof(slots));
    slot_for_each(&init_slot, 0);
}
