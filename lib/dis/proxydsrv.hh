#ifndef PROXYD_HH_
#define PROXYD_HH_

#include <inc/cpplabel.hh>

struct cobj_ref proxydsrv_create(uint64_t container, const char *name,
                label *label, label *clearance);

typedef enum
{
    proxyd_mapping,
    proxyd_local,
    proxyd_global,
} proxyd_op_t ;

struct handle_mapping
{
    char global[16]; 
    uint64_t local;
    uint64_t grant;
    uint8_t grant_level;        
};

struct proxyd_args {
    proxyd_op_t op;
    
    union {
        struct handle_mapping mapping;
        struct handle_mapping handle;    
    };    
};


#endif /*PROXYD_HH_*/
