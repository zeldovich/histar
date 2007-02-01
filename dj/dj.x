/*
 * Distributed HiStar protocol
 */

%#include <bigint.h>

typedef unsigned dj_timestamp;	/* UNIX seconds */

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
 * Labels, and labeled data (used to pass gate call arguments/responses,
 * which themselves are expected to be marshalled XDR structures).
 */

struct dj_label_entry {
    dj_gcat cat;
    unsigned level;	/* LB_LEVEL_STAR from <inc/label.h> */
};

struct dj_label {
    dj_label_entry ents<1024>;
    unsigned deflevel;
};

struct dj_gate_arg {
    opaque buf<>;
    dj_label taint;	/* taint of associated data */
    dj_gcat grant<>;	/* grant on gate invocation */
};

/*
 * Signed statements that can be made by entities.  Every network
 * message is a statement.
 *
 * Fully self-describing statements ensure that one statement
 * cannot be mistaken for another in a different context.
 */

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
    dj_timestamp from_ts;
    dj_timestamp until_ts;
};

struct dj_gatename {
    hyper gate_ct;
    hyper gate_id;
};

struct dj_call_request {
    dj_gatename gate;
    unsigned timeout_sec;
    dj_gate_arg arg;
};

enum dj_reply_status {
    REPLY_DONE = 1,
    REPLY_INPROGRESS,
    REPLY_GATE_CALL_ERROR,
    REPLY_ADDRESS_MISSING,	/* not returned by server */
    REPLY_DELEGATION_MISSING,
    REPLY_TIMEOUT,
    REPLY_SYSERR
};

union dj_call_reply switch (dj_reply_status stat) {
 case REPLY_DONE:
    dj_gate_arg arg;
 default:
    void;
};

enum dj_call_op {
    CALL_REQUEST = 1,
    CALL_REPLY,
    CALL_ABORT
};

union dj_call_u switch (dj_call_op op) {
 case CALL_REQUEST:
    dj_call_request req;
 case CALL_REPLY:
    dj_call_reply reply;
 case CALL_ABORT:
    void;
};

struct dj_call {		/* call-related message */
    hyper xid;			/* identifies the call object */
    hyper seq;			/* monotonically increasing for an xid */
    dj_timestamp ts;		/* to bound size of replay cache */
    dj_esign_pubkey from;
    dj_esign_pubkey to;
    dj_call_u u;
};

enum dj_stmt_type {
    STMT_DELEGATION = 1,
    STMT_CALL
};

union dj_stmt switch (dj_stmt_type type) {
 case STMT_DELEGATION:
    dj_delegation delegation;
 case STMT_CALL:
    dj_call call;
};

struct dj_stmt_signed {
    dj_stmt stmt;
    bigint sign;
};

