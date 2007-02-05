#ifndef JOS_INC_COOPERATE_HH
#define JOS_INC_COOPERATE_HH

extern "C" {
#include <inc/container.h>
}

#include <inc/cpplabel.hh>

typedef struct {
    union {
	uint64_t i;
	label *l;
    } u;

    char is_label;
} coop_sysarg;

/*
 * API to create and invoke cooperation gates.
 */
cobj_ref
    coop_gate_create(uint64_t container,
		     label *l,
		     label *clearance,
		     label *verify,
		     coop_sysarg arg_values[8],
		     char arg_freemask[8]);

int64_t
    coop_gate_invoke(cobj_ref coop_gate,
		     label *cs, label *ds, label *dr,
		     coop_sysarg arg_values[8]);

#endif
