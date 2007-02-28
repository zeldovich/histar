#ifndef JOS_DJ_CRYPTCONN_HH
#define JOS_DJ_CRYPTCONN_HH

#include <async.h>
#include <arpc.h>
#include <dj/djprot.hh>

class crypt_conn : virtual public refcount {
 public:
    typedef callback<void, const dj_pubkey&, const str&>::ptr rcb_t;
    crypt_conn(int fd, djprot*, rcb_t, cbv ready_cb);
    crypt_conn(int fd, dj_pubkey remote, djprot*, rcb_t, cbv ready_cb);

    void send(const str &msg);
    bool ready() { return setup_done_; }
    bool dead() { return x_ == 0; }

 private:
    void die() { x_ = 0; }
    void key_send();

    void key_recv(const char *buf, ssize_t len, const sockaddr*);
    void data_recv(const char *buf, ssize_t len, const sockaddr*);

    bool initiate_;
    dj_stmt_signed local_ss_;
    dj_pubkey remote_;
    djprot *p_;
    rcb_t cb_;
    cbv ready_cb_;

    bool setup_done_;
    ptr<axprt_crypt> x_;
};

#endif
