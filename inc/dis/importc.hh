#ifndef EXPORTCLIENT_HH_
#define EXPORTCLIENT_HH_

#include <inc/container.h>

///////
class import_segmentc
{
public:
    import_segmentc(cobj_ref gate, int id, uint64_t grant) : 
        gate_(gate), id_(id), grant_(grant) {} 
    
    int  read(void *buf, int count, int offset);
    int  write(const void *buf, int count, int offset);
    void stat(struct seg_stat *buf);
    
    cobj_ref gate(void) { return gate_; }
    int      id(void) { return id_; }

private:
    cobj_ref gate_;
    int      id_;
    uint64_t grant_;
};

//////
class import_managerc
{
public:
    import_managerc(cobj_ref gate) : gate_(gate) {} 
    
    import_segmentc segment_new(const char *host, uint16_t port, 
                                const char *path, uint64_t grant);
    void            segment_del(import_segmentc *seg, uint64_t grant);
    
private:
    cobj_ref gate_;
};

#endif /*EXPORTCLIENT_HH_*/
