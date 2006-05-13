#ifndef EXPORTD_HH_
#define EXPORTD_HH_

typedef enum {
    em_add_client,        
} export_manager_op;

struct export_manager_arg 
{
    export_manager_op op;                
    uint64_t user_grant;
    uint64_t user_taint;
    char     user_name[32];
    // return
    int      status;
    int      client_id;
    cobj_ref client_gate;       
};

typedef enum {
    ec_segment_new,     
    ec_segment_read,
    ec_segment_write,   
    ec_segment_stat,
} export_client_op;

struct export_client_arg 
{
    export_client_op op;                
    int id;
    int status;

    union {
        struct {        
            char     host[32];
            uint16_t port;
            char     path[64];    
            // return
            uint64_t remote_seg;            
        } segment_new;
        struct {
            uint64_t remote_seg;
            uint64_t taint;
            int      count;
            int      offset;
            // return
            cobj_ref seg;
        } segment_read;
        struct {
            uint64_t remote_seg;
            uint64_t taint;
            int      count;
            int      offset;
            // return
            cobj_ref seg;
        } segment_write;
        struct {
            uint64_t remote_seg;
            uint64_t taint;
            // return
            cobj_ref seg;
        } segment_stat;
    };
};

struct seg_stat {
    uint64_t ss_size;    
};

#endif /*EXPORTD_HH_*/
