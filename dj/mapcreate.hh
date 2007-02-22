#ifndef JOS_DJ_MAPCREATE_HH
#define JOS_DJ_MAPCREATE_HH

#include <dj/djprot.hh>
#include <dj/catmgr.hh>

// Processes the caller's RPC request to an EP_MAPCREATE
class histar_mapcreate {
 public:
    histar_mapcreate(djprot *p, catmgr *cm)
	: counter_(1), p_(p), cm_(cm) {}
    void exec(const dj_pubkey&, const dj_message&, const delivery_args&);

 private:
    uint64_t counter_;
    djprot *p_;
    catmgr *cm_;
};

#endif
