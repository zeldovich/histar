/*
 * Distributed HiStar protocol
 */

%#include <bigint.h>

struct dj_esign_pubkey {
    bigint n;
    unsigned k;
};

struct dj_gcat {		/* Global category name */
    dj_esign_pubkey key;
    hyper id;
};

struct dj_address {
    unsigned ip;	/* network byte order */
    unsigned port;	/* network byte order */
};

/*
 * Statements that can be made by entities (typically signed).
 * Fully self-describing statements ensure that one statement
 * cannot be mistaken for another in a different context.
 */

enum dj_stmt_type {
    STMT_DELEGATION = 1
};

enum dj_entity_type {
    ENT_PUBKEY = 1,
    ENT_GCAT,
    ENT_ADDRESS
};

union dj_entity switch (dj_entity_type type) {
 case ENT_PUBKEY:
    dj_esign_pubkey key;
 case ENT_GCAT:
    dj_gcat gcat;
 case ENT_ADDRESS:
    dj_address addr;
};

struct dj_delegation {		/* a speaks-for b, within time window */
    dj_entity a;
    dj_entity b;
    unsigned from_sec;
    unsigned until_sec;
};

union dj_stmt switch (dj_stmt_type type) {
 case STMT_DELEGATION:
    dj_delegation delegation;
};

struct dj_stmt_signed {
    dj_stmt stmt;
    bigint sign;
};

/*
 * Labels, and labeled data (used to pass gate call arguments/responses,
 * which themselves are expected to be marshalled XDR structures).
 */

struct dj_label_entry {
    dj_gcat cat;
    int level;		/* LB_LEVEL_STAR from <inc/label.h> */
};

struct dj_label {
    dj_label_entry ents<1024>;
};

struct dj_gate_buf {
    opaque buf<>;
    dj_label label;	/* label of associated data */
    dj_label grant;	/* grant on gate invocation */
    dj_label verify;	/* verify label for gate invocation */
};

/*
 * Wire message format.
 */

enum dj_wire_msg_type {
    DJ_BCAST_STMT
};

union dj_wire_msg switch (dj_wire_msg_type type) {
 case DJ_BCAST_STMT:
    dj_stmt_signed s;
};

