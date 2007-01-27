#ifndef JOS_INC_DIS_HH
#define JOS_INC_DIS_HH

#include <inc/cpplabel.hh>
#include <async.h>

class djprot : virtual public refcount {
 public:
    virtual ~djprot() {}
    virtual void set_label(const label &l) = 0;
    virtual void set_clear(const label &c) = 0;

    static ptr<djprot> alloc(uint16_t port);
};

#endif
