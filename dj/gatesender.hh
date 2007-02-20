#ifndef JOS_DJ_GATESENDER_HH
#define JOS_DJ_GATESENDER_HH

extern "C" {
#include <inc/container.h>
}

#include <dj/djprotx.h>

class gate_sender {
 public:
    gate_sender(cobj_ref djd_gate) : g_(djd_gate) {}

    dj_delivery_code send(const dj_pubkey &node, time_t timeout,
			  const dj_delegation_set &dset, const dj_catmap &cm,
			  const dj_message &msg, uint64_t *tokenp);

 private:
    cobj_ref g_;
};

#endif
