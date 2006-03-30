extern "C" {
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/netd.h>
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/base64.h>
#include <inc/authd.h>
#include <inc/gateparam.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
}

#include <inc/nethelper.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/authclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

static void __attribute__((noreturn))
httpd_worker(void *arg, gate_call_data *gcd, gatesrv_return *gr)
{
    gcd->param_buf[sizeof(gcd->param_buf) - 1] = '\0';
    char *req = &gcd->param_buf[0];

    int reply_maxlen = 4096;
    char *reply_buf = (char *) malloc(reply_maxlen);
    scope_guard<void, void*> free_buf(free, reply_buf);

    snprintf(reply_buf, reply_maxlen,
	    "HTTP/1.0 200 OK\r\n"
	    "Content-Type: text/html\r\n"
	    "\r\n"
	    "Hello world, from httpd_worker.\r\n"
	    "Request path = %s\n", req);

    int reply_len = strlen(reply_buf) + 1;
    struct cobj_ref reply_seg;
    void *va = 0;
    int r = segment_alloc(gcd->taint_container, reply_len,
			  &reply_seg, &va, 0, "worker reply");
    if (r < 0) {
	cprintf("httpd_worker: cannot allocate reply: %s\n", e2s(r));
    } else {
	memcpy(va, reply_buf, reply_len);
	segment_unmap(va);
	gcd->param_obj = reply_seg;
    }

    gr->ret(0, 0, 0);
}

int
main(int ac, char **av)
{
    label th_ctm, th_clr;
    thread_cur_label(&th_ctm);
    thread_cur_clearance(&th_clr);

    gate_create(start_env->shared_container, "worker",
		&th_ctm, &th_clr,
		&httpd_worker, 0);
    thread_halt();
}
