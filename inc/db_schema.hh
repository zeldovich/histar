#ifndef JOS_INC_DB_SCHEMA_HH
#define JOS_INC_DB_SCHEMA_HH

struct db_row {
    uint64_t dbr_id;
    char dbr_name[64];
    uint32_t dbr_zipcode;
};

#endif
