/*
 * Mapping creation service, implemented as an RPC server
 * on a special endpoint.
 */

%#include <dj/djprotx.h>

struct dj_mapreq {
    dj_gcat gcat;
    unsigned hyper lcat;
    unsigned hyper ct;
};

