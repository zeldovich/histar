/*
 * Message format for incoming requests into djd's local gate.
 */

%#include <dj/dj.h>

struct dj_incoming_gate_req {
    opaque nodepk<>;
    dj_gatename gate;
    opaque data<>;
};

union dj_incoming_gate_res switch (dj_reply_status stat) {
 case REPLY_DONE:
    opaque data<>;
 default:
    void;
};
