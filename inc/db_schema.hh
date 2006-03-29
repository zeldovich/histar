#ifndef JOS_INC_DB_SCHEMA_HH
#define JOS_INC_DB_SCHEMA_HH

extern "C" {
#include <inc/container.h>
}

struct db_row {
    // Public information visible to anyone
    uint64_t dbr_id;
    uint32_t dbr_zipcode;
    char dbr_nickname[64];

    // Private information -- not revealed to anyone else
    char dbr_name[64];

    // Private information -- can be declassified in some ways
    uint8_t dbr_match_vector[256];
};

struct db_query {
    int reqtype;
    struct cobj_ref obj;
};

struct db_reply {
    int status;
    struct cobj_ref obj;
};

#endif
