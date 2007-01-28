#ifndef JOS_INC_DIS_HH
#define JOS_INC_DIS_HH

#include <inc/cpplabel.hh>
#include <async.h>

typedef callback<void, dj_reply_status, str, label, label>::T gatecall_cb_t;

class djprot : virtual public refcount {
 public:
    virtual ~djprot() {}
    virtual str pubkey() const = 0;
    virtual void set_label(const label &l) = 0;
    virtual void set_clear(const label &c) = 0;

    virtual void gatecall(str nodepk, const dj_gatename &gate,
			  str args, label l, label grant,
			  gatecall_cb_t cb) = 0;

    static ptr<djprot> alloc(uint16_t port);
};

#endif
