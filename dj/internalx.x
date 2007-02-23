%#include <dj/djprotx.h>

/*
 * Mapping creation service, implemented as an RPC server
 * on a special endpoint.  Replies with dj_cat_mapping.
 */

struct dj_mapreq {
    dj_gcat gcat;
    unsigned hyper lcat;
    unsigned hyper ct;
};

/*
 * Delegation creation internal RPC service.
 * Replies with dj_stmt_signed.
 */

struct dj_delegate_req {
    dj_gcat gcat;
    dj_pubkey to;
    dj_timestamp from_ts;
    dj_timestamp until_ts;
};

