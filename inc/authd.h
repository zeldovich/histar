#ifndef JOS_INC_AUTHD_H
#define JOS_INC_AUTHD_H

#include <inc/types.h>
#include <inc/container.h>

// Calls to user directory service
enum {
    auth_dir_lookup,
    auth_dir_add,
    auth_dir_remove,
};

struct auth_dir_req {
    int op;
    char user[16];
    struct cobj_ref user_gate;	// for add
};

struct auth_dir_reply {
    int err;
    struct cobj_ref user_gate;
};

// Calls into user gate
struct auth_user_req {
    uint64_t req_cats;		// non-zero if just want user categories

    uint64_t pw_taint;
    uint64_t session_ct;
    uint64_t coop_gate;
};

struct auth_user_reply {
    int err;

    uint64_t ug_cat;
    uint64_t ut_cat;

    uint64_t uauth_gate;
    uint64_t ugrant_gate;
    uint64_t xh;
};

// Calls into auth gate
struct auth_uauth_req {
    uint64_t session_ct;
    char pass[16];
    char npass[16];
    uint8_t change_pw;
};

struct auth_uauth_reply {
    int err;
};

// Calls into grant gate
struct auth_ugrant_reply {
    uint64_t user_grant;
    uint64_t user_taint;
};

#endif
