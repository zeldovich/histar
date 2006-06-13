#ifndef CATC_HH_
#define CATC_HH_

class catc {
public:    
    catc(void);
    catc(uint64_t grant);
    catc(cobj_ref gate, uint64_t grant) : 
        gate_(gate), grant_(grant) {} 
        
    void     grant_cat(uint64_t local);
    cobj_ref package(const char *path);
    //uint64_t local(const char *global, bool grant);
    //void     global(uint64_t local, char *global, bool grant);

private:
    cobj_ref gate_;     
    uint64_t grant_;
};


#endif /*CATC_HH_*/
