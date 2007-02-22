#ifndef JOS_DJ_GATEEXEC_HH
#define JOS_DJ_GATEEXEC_HH

#include <dj/djprot.hh>
#include <dj/catmgr.hh>

void gate_exec(catmgr*, cobj_ref djd_gate,
	       const dj_pubkey&, const dj_message&, const delivery_args&);

#endif
