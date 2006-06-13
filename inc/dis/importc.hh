#ifndef EXPORTCLIENT_HH_
#define EXPORTCLIENT_HH_

#include <inc/container.h>

///////
class import_segmentc
{
public:
    import_segmentc(cobj_ref gate, const char *path); 
    
    int  read(void *buf, int count, int offset);
    int  write(const void *buf, int count, int offset);
    void stat(struct seg_stat *buf);
    
    cobj_ref gate(void) { return gate_; }

private:
    char *   path_;
    cobj_ref gate_;
};

//////
class import_managerc
{
public:
    import_managerc(cobj_ref manager_gt, cobj_ref wrap_gt) 
        : manager_gt_(manager_gt), wrap_gt_(wrap_gt) {} 
    
    import_segmentc *segment_new(const char *path);
    void             segment_del(import_segmentc *seg);
    
private:
    cobj_ref manager_gt_;
    cobj_ref wrap_gt_;
};

#endif /*EXPORTCLIENT_HH_*/
