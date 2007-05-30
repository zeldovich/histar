#ifndef JOS_DJ_MAPCREATE_HH
#define JOS_DJ_MAPCREATE_HH

extern "C" {
#include <inc/bf60.h>
}

#include <dj/djprot.hh>
#include <dj/catmgr.hh>

struct local_mapcreate_state {
    label *vl;
    label *vc;
    cobj_ref privgate;
};

// Processes the caller's RPC request to an EP_MAPCREATE
class histar_mapcreate {
 public:
    histar_mapcreate(djprot *p, catmgr *cm);
    void exec(const dj_message&, const delivery_args&);

 private:
    uint64_t counter_;
    struct bf_ctx bf_ctx_;

    djprot *p_;
    catmgr *cm_;
};

#endif
