#ifndef REMFILEDSRV_HH_
#define REMFILEDSRV_HH_

#include <inc/cpplabel.hh>

struct cobj_ref remfiledsrv_create(uint64_t container, label *label, 
                                   label *clearance);

typedef enum
{
    remfile_read,
    remfile_write,
} remfiled_op_t ;

struct remfiled_args {
    remfiled_op_t op;
    rem_inode ino;
    uint64_t count;
    uint64_t off;
    
    uint64_t grant; 
    uint64_t taint;   
    cobj_ref seg;
};

#endif /*REMFILEDSRV_HH_*/
