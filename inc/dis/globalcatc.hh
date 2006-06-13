#ifndef GLOBALCATC_HH_
#define GLOBALCATC_HH_

class global_catc {
public:    
    global_catc(void);
    global_catc(uint64_t grant);
    global_catc(cobj_ref gate, uint64_t grant) : 
        gate_(gate), grant_(grant) {} 
        
    //uint64_t local(const char *global, bool grant);
    //void     global(uint64_t local, char *global, bool grant);
    //void     global_is(uint64_t local, const char *global);

    uint64_t foreign(struct global_cat global); 
    label*   foreign_label(global_label *gl);

private:
    cobj_ref gate_;     
    uint64_t grant_; 
};


#endif /*GLOBALCATC_HH_*/
