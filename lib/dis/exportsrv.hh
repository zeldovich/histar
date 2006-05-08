#ifndef JOS_INC_EXPORTSRV_HH_
#define JOS_INC_EXPORTSRV_HH_

#include <inc/cpplabel.hh>

void exportsrv_start(uint64_t container, label *la, 
                    label *clearance);

void export_acquire(uint64_t taint);

typedef enum {
    exp_grant,    
} export_op_t;

struct export_args
{
    export_op_t op;
    uint64_t grant;
    uint64_t handle;
    int ret;
};

#endif /*JOS_INC_EXPORTSRV_HH_*/
