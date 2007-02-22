#ifndef JOS_DJ_GATESENDER_HH
#define JOS_DJ_GATESENDER_HH

extern "C" {
#include <inc/container.h>
#include <inc/lib.h>
}

#include <inc/error.hh>
#include <dj/djprotx.h>

class gate_sender {
 public:
    gate_sender() {
	int64_t gate_ct, gate_id;
	error_check(gate_ct = container_find(start_env->root_container,
					     kobj_container, "djd"));
	error_check(gate_id = container_find(gate_ct,
					     kobj_gate, "djd-incoming"));
	g_ = COBJ(gate_ct, gate_id);
    }

    gate_sender(cobj_ref djd_gate) : g_(djd_gate) {}

    dj_delivery_code send(const dj_pubkey &node, time_t timeout,
			  const dj_delegation_set &dset, const dj_catmap &cm,
			  const dj_message &msg, uint64_t *tokenp,
			  label *grantlabel = 0);

 private:
    cobj_ref g_;
};

#endif
