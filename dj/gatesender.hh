#ifndef JOS_DJ_GATESENDER_HH
#define JOS_DJ_GATESENDER_HH

extern "C" {
#include <inc/container.h>
#include <inc/lib.h>
}

#include <inc/error.hh>
#include <inc/cpplabel.hh>
#include <dj/djprotx.h>
#include <dj/hsutil.hh>

class gate_sender {
 public:
    gate_sender() {
	int64_t gate_ct, gate_id;
	error_check(gate_ct = container_find(start_env->root_container,
					     kobj_container, "djd"));
	error_check(gate_id = container_find(gate_ct,
					     kobj_gate, "djd-incoming"));
	g_ = COBJ(gate_ct, gate_id);

	int64_t key_sg;
	error_check(key_sg = container_find(gate_ct, kobj_segment, "selfkey"));
	str keystr = segment_to_str(COBJ(gate_ct, key_sg));
	assert(str2xdr(hostkey_, keystr));
    }

    gate_sender(cobj_ref djd_gate) : g_(djd_gate) {}

    dj_delivery_code send(const dj_delegation_set &dset, const dj_catmap &cm,
			  const dj_message &msg, label *grantlabel = 0);
    dj_pubkey hostkey() { return hostkey_; }

 private:
    cobj_ref g_;
    dj_pubkey hostkey_;
};

#endif
