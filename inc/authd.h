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






enum {
    // Calls into the main authd gate
    authd_adduser,
    authd_deluser,
    authd_getuser,

    // Calls into the user gate
    authd_login,
    authd_chpass,
};

struct authd_req {
    int op;
    char user[16];
    char pass[16];
    char npass[16];
};

struct authd_reply {
    int err;

    uint64_t user_id;
    uint64_t user_taint;
    uint64_t user_grant;

    struct cobj_ref user_gate;
};

#endif
