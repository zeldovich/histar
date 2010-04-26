#ifndef JOS_INC_RILD_H
#define JOS_INC_RILD_H

#include <inc/container.h>

enum {
	radio_on,
	radio_off,
	send_sms,
	dial_number,
	answer_call,
	end_call,
	default_pdp_on,
	get_ril_interface_version,
	get_ril_lib_version,
	get_ril_state,
	neighboring_cells,
	registration_status
};

struct rild_req {
	int op;
	struct cobj_ref obj;
	int bufbytes;
	union {
		char buf[256];
	};
};

struct rild_reply {
	int err;
	int bufbytes;
	union {
		char buf[256];
	};
};

#endif /* !JOS_INC_RILD_H */
