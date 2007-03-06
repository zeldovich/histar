#ifndef JOS_DJ_EXECMUX_HH
#define JOS_DJ_EXECMUX_HH

#include <dj/djprot.hh>
#include <dj/djops.hh>
#include <map>

class exec_mux {
 public:
    exec_mux() {}

    void set(dj_endpoint_type t, djprot::local_delivery_cb cb) {
	m_[t] = cb;
    }

    void exec(const dj_pubkey &pk, const dj_message &m, const delivery_args &a) {
	std::map<dj_endpoint_type, djprot::local_delivery_cb>::iterator i =
	    m_.find(m.target.type);
	if (i == m_.end()) {
	    warn << "exec_mux: no delivery for type " << m.target.type << "\n";
	    a.cb(DELIVERY_REMOTE_ERR);
	    return;
	}

	i->second(pk, m, a);
    }

 private:
    std::map<dj_endpoint_type, djprot::local_delivery_cb> m_;
};

#endif
