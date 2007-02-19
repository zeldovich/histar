#ifndef JOS_INC_DJPROT_HH
#define JOS_INC_DJPROT_HH

#include <async.h>
#include <dj/dj.h>

typedef callback<void, dj_delivery_code, uint64_t>::ptr delivery_status_cb;

class message_sender {
 public:
    virtual ~message_sender() {}
    virtual void send(const dj_esign_pubkey &node, time_t timeout,
		      const dj_delegation_set &dels,
		      const dj_message &msg, delivery_status_cb cb) = 0;
};

class djprot : public message_sender {
 public:
    typedef callback<void, const dj_esign_pubkey&, const dj_message&,
			   delivery_status_cb>::ptr local_delivery_cb;

    virtual dj_esign_pubkey pubkey() const = 0;
    virtual void set_label(const dj_label &l) = 0;
    virtual void set_clear(const dj_label &c) = 0;
    virtual void set_delivery_cb(local_delivery_cb cb) = 0;

    static djprot *alloc(uint16_t port);
};

#endif
