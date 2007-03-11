%#include <dj/djprotx.h>

/*
 * Mapping creation service, implemented as an RPC server
 * on a special endpoint.
 */

struct dj_mapreq {
    dj_gcat gcat;
    unsigned hyper lcat;
    unsigned hyper ct;
};

struct dj_mapcreate_arg {
    dj_mapreq reqs<>;
};

struct dj_mapcreate_res {
    dj_cat_mapping mappings<>;
};

/*
 * Delegation creation internal RPC service.
 */

struct dj_delegate_req {
    dj_gcat gcat;
    dj_pubkey to;
    dj_timestamp from_ts;
    dj_timestamp until_ts;
};

struct dj_delegate_arg {
    dj_delegate_req reqs<>;
};

struct dj_delegate_res {
    dj_stmt_signed delegations<>;
};

