extern "C" {
#include <inc/signal.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/setjmp.h>
#include <inttypes.h>
}

#include <inc/error.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

void
segfault_helper(siginfo_t *si, struct sigcontext *sc)
{
    void *va = si->si_addr;

    try {
	u_segment_mapping usm;
	int r = segment_lookup(va, &usm);

	cprintf("%s: fatal signal %d, addr=%p [tid=%"PRIu64", pid=%"PRIu64"]\n",
		jos_progname, si->si_signo, si->si_addr,
		sys_self_id(), start_env->shared_container);
	if (sc)
	    cprintf("%s: rip=0x%zx, rsp=0x%zx\n",
		    jos_progname, sc->sc_utf.utf_pc, sc->sc_utf.utf_stackptr);

	if (r < 0)
	    throw error(r, "segment_lookup");

	char name[KOBJ_NAME_LEN];
	name[0] = '\0';
	sys_obj_get_name(usm.segment, &name[0]);

	label seg_label;
	obj_get_label(usm.segment, &seg_label);

	label cur_label;
	thread_cur_label(&cur_label);

	cprintf("%s: segfault_helper: VA %p, segment %"PRIu64".%"PRIu64" (%s), flags %x\n",
		jos_progname, va,
		usm.segment.container, usm.segment.object,
		&name[0], usm.flags);
	cprintf("%s: segfault_helper: segment label %s, thread label %s\n",
		jos_progname, seg_label.to_string(), cur_label.to_string());
    } catch (std::exception &e) {
	cprintf("%s: segfault_helper: %s\n", jos_progname, e.what());
    }
}
