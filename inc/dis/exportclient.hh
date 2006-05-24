#ifndef EXPORTCLIENT_HH_
#define EXPORTCLIENT_HH_

#include <inc/container.h>

///////
class export_segmentc
{
public:
    export_segmentc(cobj_ref gate, int id, uint64_t grant) : 
        gate_(gate), id_(id), grant_(grant) {} 
    
    int  read(void *buf, int count, int offset);
    int  write(const void *buf, int count, int offset);
    void stat(struct seg_stat *buf);
    void close(void);
    
    cobj_ref gate(void) { return gate_; }
    int      id(void) { return id_; }

private:
    cobj_ref gate_;
    int      id_;
    uint64_t grant_;
};

///////
class export_clientc
{
public:
    export_clientc(cobj_ref gate, int id, uint64_t grant) : 
        gate_(gate) , id_(id), grant_(grant) {} 
    
    export_segmentc segment_new(const char *host, uint16_t port, const char *path);
    export_segmentc segment_del(export_segmentc seg);
    export_segmentc segment(const char *host, uint16_t port, const char *path);
    
private:
    cobj_ref gate_;
    int      id_;
    uint64_t grant_;
};

//////
class export_managerc
{
public:
    export_managerc(cobj_ref gate) : gate_(gate) {} 
    export_clientc client_new(char *name, uint64_t grant);
    void           client_del(char *name);
    export_clientc client(char *name);
    
    export_segmentc segment_new(const char *host, uint16_t port, 
                                const char *path, uint64_t grant);
    void            segment_del(export_segmentc *seg);
    
private:
    cobj_ref gate_;
};

#endif /*EXPORTCLIENT_HH_*/
