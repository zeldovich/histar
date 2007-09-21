#ifndef JOS_DJ_CATMGR_HH
#define JOS_DJ_CATMGR_HH

#include <dj/djprotx.h>
#include <dj/reqcontext.hh>

class catmgr {
 public:
    virtual ~catmgr() {}
    virtual void acquire(const dj_catmap&, bool droplater = false) = 0;
    virtual dj_cat_mapping store(const dj_gcat&, uint64_t lcat, uint64_t uct) = 0;
    virtual void drop_later(uint64_t cat) = 0;
    virtual void drop_now() = 0;

    static catmgr* alloc();
};

#endif
