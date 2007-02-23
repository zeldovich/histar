/*
 * Mapping creation service, implemented as an RPC server
 * on a special endpoint.
 */

%#include <dj/djprotx.h>

struct dj_mapreq_proof {
    dj_privkey privkey;
    dj_delegation_set dset;
};

struct dj_mapreq {
    dj_mapreq_proof *proof;
    dj_gcat gcat;
    unsigned hyper lcat;
    unsigned hyper ct;
};

