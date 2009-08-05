#ifndef JOS_INC_SMSD_H
#define JOS_INC_SMSD_H

#include <inc/container.h>

enum {
	incoming_sms,
	outgoing_sms
};

struct smsd_req {
	int op;
	char buf[40];
	char buf2[160];
};

struct smsd_reply {
	int err;
};

#endif /* !JOS_INC_SMSD_H */
