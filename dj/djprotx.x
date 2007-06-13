/*
 * Distributed HiStar protocol
 */

%#include <sfs_prot.h>

typedef unsigned dj_timestamp;	/* UNIX seconds */
typedef opaque dj_stmt_blob<>;	/* No recursive definitions in XDR */
typedef sfs_pubkey2 dj_pubkey;
typedef sfs_sig2 dj_sign;

struct dj_gcat {		/* Global category name */
    dj_pubkey key;
    unsigned hyper id;
    unsigned integrity;
};

struct dj_address {
    unsigned ip;		/* network byte order */
    unsigned port;		/* network byte order */
};

/*
 * Labels.
 */

struct dj_label {
    dj_gcat ents<>;
};

struct dj_cat_mapping {
    dj_gcat gcat;
    unsigned hyper lcat;

    unsigned hyper res_ct;	/* sub-container used to store mapping */
    unsigned hyper res_gt;	/* unbound gate providing { lcat* } */
};

struct dj_catmap {
    dj_cat_mapping ents<>;
};

/*
 * Delegations.
 */

enum dj_entity_type {
    ENT_PUBKEY = 1,
    ENT_GCAT,
    ENT_ADDRESS
};

union dj_entity switch (dj_entity_type type) {
 case ENT_PUBKEY:
    dj_pubkey key;
 case ENT_GCAT:
    dj_gcat gcat;
 case ENT_ADDRESS:
    dj_address addr;
};

struct dj_delegation {		/* via says a speaks-for b, within time window */
    dj_entity a;
    dj_entity b;
    dj_pubkey *via;
    dj_timestamp from_ts;
    dj_timestamp until_ts;
};

struct dj_delegation_set {
    dj_stmt_blob ents<>;	/* really XDR-encoded dj_signed_stmt */
};

/*
 * Message transfer.
 */

enum dj_slot_type {
    EP_GATE = 1,
    EP_SEGMENT,
    EP_MAPCREATE,
    EP_DELEGATOR
};

enum dj_special_gate_ids {	/* set container to zero */
    GSPEC_CTALLOC = 1,
    GSPEC_ECHO,
    GSPEC_GUARDCALL
};

struct dj_gatename {
    unsigned hyper gate_ct;
    unsigned hyper gate_id;
};

struct dj_ep_gate {
    unsigned hyper msg_ct;	/* container ID for message segment & thread */
    dj_gatename gate;
};

struct dj_ep_segment {
    unsigned hyper seg_ct;
    unsigned hyper seg_id;
};

union dj_slot switch (dj_slot_type type) {
 case EP_GATE:
    dj_ep_gate ep_gate;
 case EP_SEGMENT:
    dj_ep_segment ep_segment;
 case EP_MAPCREATE:
    void;
 case EP_DELEGATOR:
    void;
};

struct dj_message {
    dj_pubkey from;
    dj_pubkey to;

    dj_slot target;		/* gate or segment to call on delivery */
    dj_label taint;		/* taint of message */
    dj_label glabel;		/* grant label on gate invocation */
    dj_label gclear;		/* grant clearance on gate invocation */
    dj_catmap catmap;		/* target node category mappings */
    dj_delegation_set dset;	/* supporting delegations */
    opaque msg<>;
};

/*
 * Not all message delivery codes are exposed to the sender.
 */

enum dj_delivery_code {
    DELIVERY_DONE = 1,
    DELIVERY_TIMEOUT,
    DELIVERY_NO_ADDRESS,
    DELIVERY_LOCAL_DELEGATION,
    DELIVERY_REMOTE_DELEGATION,
    DELIVERY_LOCAL_MAPPING,
    DELIVERY_REMOTE_MAPPING,
    DELIVERY_LOCAL_ERR,
    DELIVERY_REMOTE_ERR
};

/*
 * Session key establishment.
 */

struct dj_key_setup {
    dj_pubkey sender;
    dj_pubkey to;
    sfs_ctext2 kmsg;
};

/*
 * Signed statements that can be made by entities.  Every network
 * message is a statement.
 *
 * Fully self-describing statements ensure that one statement
 * cannot be mistaken for another in a different context.
 */

enum dj_stmt_type {
    STMT_DELEGATION = 1,
    STMT_MSG,
    STMT_KEY_SETUP
};

union dj_stmt switch (dj_stmt_type type) {
 case STMT_DELEGATION:
    dj_delegation delegation;
 case STMT_MSG:
    dj_message msg;
 case STMT_KEY_SETUP:
    dj_key_setup keysetup;
};

struct dj_stmt_signed {
    dj_stmt stmt;
    dj_sign sign;
};

