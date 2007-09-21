#ifndef JOS_DJ_GATEINCOMING_HH
#define JOS_DJ_GATEINCOMING_HH

#include <dj/djprot.hh>
#include <dj/catmgr.hh>

class dj_incoming_gate {
 public:
    virtual ~dj_incoming_gate() {}
    virtual cobj_ref gate() = 0;

    static dj_incoming_gate *alloc(djprot*, catmgr*, uint64_t ct);
};

#endif
