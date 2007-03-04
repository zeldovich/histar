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

/*
 * Remote authentication/login.
 */

struct authproxy_arg {
    string username<16>;
    string password<16>;
    unsigned hyper map_ct;
    unsigned hyper return_map_ct;
};

struct authproxy_resok {
    dj_stmt_signed ut_delegation;
    dj_stmt_signed ug_delegation;
    dj_cat_mapping ut_local;
    dj_cat_mapping ug_local;
    dj_cat_mapping ut_remote;
    dj_cat_mapping ug_remote;
};

union authproxy_res switch (bool ok) {
 case TRUE:
    authproxy_resok resok;
 case FALSE:
    void;
};

/*
 * Remote web server application execution.
 */

struct webapp_arg {
    dj_cat_mapping ug_map_apphost;
    dj_cat_mapping ut_map_apphost;
    dj_cat_mapping ug_map_userhost;
    dj_cat_mapping ut_map_userhost;

    dj_stmt_signed ug_dlg_apphost;
    dj_stmt_signed ut_dlg_apphost;
    dj_stmt_signed ug_dlg_userhost;
    dj_stmt_signed ut_dlg_userhost;

    dj_pubkey userhost;
    dj_message_endpoint user_fs;

    string reqpath<>;
};

struct webapp_res {
    opaque httpres<>;
};

/*
 * Guarded calls.  For now it doesn't support gate invocation
 * and checksumming, and instead only does ELF invocation, with
 * the checksum being over the ELF binary instead of the address
 * space layout defined by the ELF binary.
 */

struct wrapped_string {		/* To avoid double-deref syntax */
    string s<>;
};

struct guardcall_arg {
    unsigned hyper parent_ct;
    string elf_pn<>;
    wrapped_string args<>;
    opaque sha1sum[20];

    dj_label taint;
    dj_label glabel;
    dj_label gclear;
};

struct guardcall_resok {
    hyper spawn_ct;
};

union guardcall_res switch (bool ok) {
 case TRUE:
    guardcall_resok resok;
 case FALSE:
    void;
};

