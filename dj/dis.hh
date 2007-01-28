#ifndef JOS_INC_DIS_HH
#define JOS_INC_DIS_HH

#include <inc/cpplabel.hh>
#include <async.h>
#include <dj/dj.h>

class djprot : virtual public refcount {
 public:
    struct gatecall_args {
	str data;
	label taint;
	label grant;
    };

    typedef callback<void, dj_reply_status, const gatecall_args&>::ptr gatecall_cb_t;

    virtual ~djprot() {}
    virtual str pubkey() const = 0;
    virtual void set_label(const label &l) = 0;
    virtual void set_clear(const label &c) = 0;

    virtual void gatecall(str nodepk, const dj_gatename &gate,
			  const gatecall_args &args, gatecall_cb_t cb) = 0;

    static ptr<djprot> alloc(uint16_t port);
};

#endif
