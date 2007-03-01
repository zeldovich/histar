%#include <dj/djprotx.h>

/*
 * Container allocation service.
 */

struct container_alloc_req {
    uint64_t parent;
    uint64_t quota;
    uint64_t timeout_msec;
    dj_label label;
};

struct container_alloc_res {
    hyper ct_id;	/* negative is error code */
};

/*
 * Perl running service.
 */

struct perl_run_arg {
    string script<>;
    string input<>;
};

struct perl_run_res {
    int retval;
    string output<>;
};

