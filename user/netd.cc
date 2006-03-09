extern "C" {
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/netd.h>
}

#include <inc/gatesrv.hh>
#include <inc/netdsrv.hh>
#include <inc/labelutil.hh>

static int netd_debug = 0;
static int netd_force_taint = 0;

static label *
force_taint_prepare(uint64_t taint)
{
    label *l = new label();
    thread_cur_label(l);

    level_t taint_level = netd_force_taint ? 2 : LB_LEVEL_STAR;
    l->set(taint, taint_level);

    segment_set_default_label(l->to_ulabel());
    int r = heap_relabel(l->to_ulabel());
    if (r < 0)
	panic("cannot relabel heap: %s", e2s(r));

    return l;
}

static void
force_taint_commit(label *l)
{
    int r = label_set_current(l->to_ulabel());
    if (r < 0)
	panic("cannot reset label to %s: %s", l->to_string(), e2s(r));

    if (netd_debug)
	printf("netd: switched to label %s\n", l->to_string());
}

static void
ready_cb(void *arg)
{
    gatesrv *srv = (gatesrv *) arg;
    srv->enable();
    printf("netd: ready\n");
}

int
main(int ac, char **av)
{
    if (ac != 3) {
	printf("Usage: %s grant-handle taint-handle\n", av[0]);
	return -1;
    }

    uint64_t grant, taint;
    int r = strtou64(av[1], 0, 10, &grant);
    if (r < 0)
	panic("parsing grant handle %s: %s", av[1], e2s(r));

    r = strtou64(av[2], 0, 10, &taint);
    if (r < 0)
	panic("parsing taint handle %s: %s", av[2], e2s(r));

    if (netd_debug)
	printf("netd: grant handle %ld, taint handle %ld\n",
	       grant, taint);

    gatesrv *srv;
    try {
	label *l = force_taint_prepare(taint);
	label clear(2);

	srv = netd_server_init(start_env->shared_container,
			       start_env->proc_container,
			       taint,
			       l, &clear);

	force_taint_commit(l);
    } catch (std::exception &e) {
	panic("%s", e.what());
    }

    netd_lwip_init(&ready_cb, srv);
}
