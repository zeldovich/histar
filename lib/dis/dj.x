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
    unsigned ip;
    unsigned port;
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

