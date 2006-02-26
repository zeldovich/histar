extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/setjmp.h>
}

#include <inc/gateclnt.hh>
#include <inc/gateparam.hh>
#include <inc/error.hh>

void
gate_call(struct cobj_ref gate, struct cobj_ref *param,
	  struct ulabel *label, struct ulabel *clearance)
{
    struct cobj_ref tseg = COBJ(kobject_id_thread_ct, kobject_id_thread_sg);
    void *tls = 0;
    error_check(segment_map(tseg, SEGMAP_READ | SEGMAP_WRITE, &tls, 0));

    //struct jmp_buf back_from_call;
    
}
