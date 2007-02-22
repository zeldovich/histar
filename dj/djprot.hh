#ifndef JOS_DJ_DJPROT_HH
#define JOS_DJ_DJPROT_HH

#include <async.h>
#include <dj/djprotx.h>

typedef callback<void, dj_delivery_code, uint64_t>::ptr delivery_status_cb;

struct delivery_args {
    delivery_status_cb cb;
    void *local_delivery_arg;
};

class message_sender {
 public:
    virtual ~message_sender() {}
    virtual void send(const dj_pubkey &node, time_t timeout,
		      const dj_delegation_set &dset,
		      const dj_message &msg, delivery_status_cb cb,
		      void *delivery_arg) = 0;
};

class djprot : public message_sender {
 public:
    typedef callback<void, const dj_pubkey&, const dj_message&,
			   const delivery_args&>::ptr local_delivery_cb;

    virtual dj_pubkey pubkey() const = 0;
    virtual void set_label(const dj_label &l) = 0;
    virtual void set_clear(const dj_label &c) = 0;
    virtual void set_delivery_cb(local_delivery_cb cb) = 0;

    static djprot *alloc(uint16_t port);
};

#endif
