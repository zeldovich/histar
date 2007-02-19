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

class request_context {
 public:
    virtual ~request_context() {}
    virtual bool can_read(uint64_t ct) = 0;
    virtual bool can_rw(uint64_t ct) = 0;
};

class catmgr {
 public:
    virtual ~catmgr() {}
    virtual dj_cat_mapping alloc(request_context*, const dj_gcat&,
				 uint64_t ct) = 0;
    virtual dj_cat_mapping store(request_context*, const dj_gcat&,
				 uint64_t lcat, uint64_t ct) = 0;
    virtual void acquire(request_context*, const dj_cat_mapping &m,
			 bool droplater = false) = 0;
};

class djprot : public message_sender {
 public:
    typedef callback<void, const dj_message&, delivery_status_cb>::ptr local_delivery_cb;

    virtual ~djprot() {}
    virtual dj_esign_pubkey pubkey() const = 0;
    virtual void set_label(const dj_label &l) = 0;
    virtual void set_clear(const dj_label &c) = 0;
    virtual void set_delivery_cb(local_delivery_cb cb) = 0;

    static djprot *alloc(uint16_t port);
};

#endif
