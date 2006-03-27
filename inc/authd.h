#ifndef JOS_INC_AUTHD_H
#define JOS_INC_AUTHD_H

#include <inc/types.h>

enum {
    authd_login,
    authd_adduser,
    authd_deluser,
    authd_chpass,
    authd_addgroup,
    authd_addtogroup,
    authd_delfromgroup,
    authd_logingroup,
    authd_getuid,
    authd_unamehandles,
};

enum {
    group_read,
    group_write,
};

struct authd_req {
    int op;
    union {
        struct {
            char user[16];
            char pass[16];
            char npass[16];
        };
        struct {
            char group[16];    
            int type;
            uint64_t grant;
            uint64_t taint;
        };
    };
};

struct authd_reply {
    int err;
    uint64_t user_id;
    uint64_t user_taint;
    uint64_t user_grant;
};

#endif
