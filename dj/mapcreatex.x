/*
 * Mapping creation service, implemented as an RPC server
 * on a special endpoint.
 */

%#include <dj/djprotx.h>

enum dj_mapreq_type {
    MAPREQ_CREATE_LOCAL = 1,
    MAPREQ_CREATE_GLOBAL
};

union dj_mapreq switch (dj_mapreq_type type) (
 case MAPREQ_CREATE_LOCAL:
    dj_gcat gcat;
 case MAPREQ_CREATE_GLOBAL:
    unsigned hyper lcat;
};

