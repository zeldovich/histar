#ifndef JOS_DJ_GATEEXEC_HH
#define JOS_DJ_GATEEXEC_HH

#include <dj/djprot.hh>
#include <dj/catmgr.hh>

void gate_exec(catmgr*, const dj_pubkey&, const dj_message&, delivery_status_cb);

#endif
