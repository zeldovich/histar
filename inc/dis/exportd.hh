#ifndef EXPORTD_HH_
#define EXPORTD_HH_

typedef enum {
    em_new_iseg,      
    em_del_iseg,
    em_new_eseg,
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

struct seg_stat {
    uint64_t ss_size;    
};

typedef enum {
    ic_segment_read,
    ic_segment_write,   
    ic_segment_stat,
} import_client_op;

struct import_client_arg 
{
    import_client_op op;                
    char path[32];    

    // return
    int status;

    union {
        struct {
            int count;
            int offset;
            // return
            cobj_ref seg;
        } segment_read;
        struct {
            cobj_ref seg;
            int      count;
            int      offset;
        } segment_write;
        struct {
            // return
            struct seg_stat stat;
        } segment_stat;
    };
};

#endif /*EXPORTD_HH_*/
