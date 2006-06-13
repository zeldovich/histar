#ifndef GLOBALCATD_HH_
#define GLOBALCATD_HH_

typedef enum {
    gcd_g2f,
} gcd_op_t;

struct gcd_arg 
{
    gcd_op_t op;
    
    union {
        struct {
            struct global_cat global;
            // return
            uint64_t foreign;    
        } f2g;
    };
    // return
    int status;
};

#endif /*GLOBALCATD_HH_*/
