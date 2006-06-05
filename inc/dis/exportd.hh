#ifndef EXPORTD_HH_
#define EXPORTD_HH_

typedef enum {
    em_new_segment,      
    em_del_segment,
} export_manager_op;

struct export_manager_arg 
{
    export_manager_op op;                
    uint64_t user_grant;
    
    char     host[32];
    uint16_t port;
    char     path[32];    

    // return
    int      status;
    int      client_id;
    cobj_ref client_gate;       
};

typedef enum {
    ic_segment_new,     
    ic_segment_read,
    ic_segment_write,   
    ic_segment_stat,
    ic_segment_close,
} import_client_op;

struct import_client_arg 
{
    import_client_op op;                
    int id;
    int status;

    union {
        struct {        
            char     host[32];
            uint16_t port;
            char     path[32];    
            // return
            cobj_ref gl_seg;
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
