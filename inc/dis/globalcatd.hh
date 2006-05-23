#ifndef GLOBALCATD_HH_
#define GLOBALCATD_HH_

typedef enum {
    gcd_add,
    gcd_rem,
    gcd_to_global,    
    gcd_to_local,
} gcd_op_t;

struct gcd_arg 
{
    gcd_op_t op;
    char global[16];
    uint64_t local;
    uint64_t clear;
    // return
    int status;
    cobj_ref grant_gt;
};

#endif /*GLOBALCATD_HH_*/
