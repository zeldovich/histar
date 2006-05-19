extern "C" {
#include <inc/signal.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/setjmp.h>
}

#include <inc/error.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

void
segfault_helper(siginfo_t *si, struct sigcontext *sc)
{
    extern const char *__progname;
    void *va = si->si_addr;

    try {
	u_segment_mapping usm;
	int r = segment_lookup(va, &usm);

	cobj_ref seg = usm.segment;
	uint64_t flags = usm.flags;
	if (r > 0 && (flags & SEGMAP_VECTOR_PF)) {
	    assert(*tls_pgfault);
	    jos_longjmp(*tls_pgfault, 1);
	}

	cprintf("%s: fatal signal %d, addr=%p\n",
		__progname, si->si_signo, si->si_addr);
	if (sc)
	    cprintf("%s: rip=0x%lx, rsp=0x%lx\n",
		    __progname, sc->sc_utf.utf_rip, sc->sc_utf.utf_rsp);

	if (r < 0)
	    throw error(r, "segment_lookup");

	char name[KOBJ_NAME_LEN];
	name[0] = '\0';
	sys_obj_get_name(seg, &name[0]);

	label seg_label;
	obj_get_label(seg, &seg_label);

	label cur_label;
	thread_cur_label(&cur_label);

	cprintf("%s: segfault_helper: VA %p, segment %ld.%ld (%s), flags %lx\n",
		__progname, va, seg.container, seg.object, &name[0], flags);
	cprintf("%s: segfault_helper: segment label %s, thread label %s\n",
		__progname, seg_label.to_string(), cur_label.to_string());
    } catch (std::exception &e) {
	cprintf("%s: segfault_helper: %s\n", __progname, e.what());
    }
}
