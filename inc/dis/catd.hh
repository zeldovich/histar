#ifndef CATD_HH_
#define CATD_HH_

typedef enum {
    cd_add,
    cd_rem,
    cd_package,
    cd_write,
} cd_op_t;

struct cd_arg 
{
    cd_op_t  op;
    union {
        struct {
            uint64_t local;
        } add;
        struct {
            uint64_t cipher_ct;
            char     path[32];
            // return
            cobj_ref seg;
        } package;
        struct {
            char     path[32];
            int      len;
            int      off;
            cobj_ref seg;
        } write;
    };
    // return
    int status;
};

#endif /*CATD_HH_*/
