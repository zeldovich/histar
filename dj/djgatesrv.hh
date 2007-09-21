#ifndef JOS_DJ_DJGATESRV_HH
#define JOS_DJ_DJGATESRV_HH

extern "C" {
#include <inc/gateparam.h>
}

#include <inc/cpplabel.hh>
#include <inc/gatesrv.hh>
#include <dj/djgate.h>

void djgate_incoming(gate_call_data *gcd,
		     const label &vl, const label &vc,
		     dj_outgoing_gate_msg *m,
		     gatesrv_return *ret);

#endif
