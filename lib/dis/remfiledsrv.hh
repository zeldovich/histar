#ifndef JOS_INC_REMFILEDSRV_HH_
#define JOS_INC_REMFILEDSRV_HH_

#include <inc/cpplabel.hh>

struct cobj_ref remfiledsrv_create(uint64_t container, label *label, 
                                   label *clearance);

typedef enum
{
    rf_read,
    rf_write,
    rf_open,
} remfiled_op_t ;

struct remfiled_args {
    remfiled_op_t op;
    rem_inode ino;
    int64_t count;
    uint64_t off;
    
    char host[16];
    char path[16];
    int port;
    
    uint64_t grant; 
    uint64_t taint;   
    cobj_ref seg;
};

#endif /*JOS_INC_REMFILEDSRV_HH_*/
