#ifndef EXPORTC_HH_
#define EXPORTC_HH_

#include <inc/container.h>

///////
class export_segmentc
{
public:
    int  read(void *buf, int count, int offset);
    int  write(const void *buf, int count, int offset);
    void stat(struct seg_stat *buf);
private:
};

//////
class export_managerc
{
public:
    export_managerc(void); 
    export_managerc(cobj_ref gate) : gate_(gate) {} 
    
    export_segmentc segment_new(const char *pn);
    
private:
    cobj_ref gate_;
};


#endif /*EXPORTC_HH_*/
