#ifndef JOS_DJ_CRYPTCONN_HH
#define JOS_DJ_CRYPTCONN_HH

#include <async.h>
#include <arpc.h>
#include <dj/djprot.hh>

typedef enum {
    crypt_cannot_connect,
    crypt_connected,
    crypt_disconnected
} crypt_conn_status;

class crypt_conn {
 public:
    typedef callback<void, const dj_pubkey&, const str&>::ptr rcb_t;
    typedef callback<void, crypt_conn*, crypt_conn_status>::ptr readycb_t;

    crypt_conn(int fd, djprot*, rcb_t, readycb_t ready_cb);
    crypt_conn(int fd, dj_pubkey remote, djprot*, rcb_t, readycb_t ready_cb);

    void send(const str &msg);

    // For ihash
    dj_pubkey remote_;
    ihash_entry<crypt_conn> link_;

 private:
    void die(crypt_conn_status code) { ready_cb_(this, code); }
    void key_send();

    void key_recv(const char *buf, ssize_t len, const sockaddr*);
    void data_recv(const char *buf, ssize_t len, const sockaddr*);

    bool initiate_;
    sfs_kmsg local_kmsg_;
    djprot *p_;
    rcb_t cb_;
    readycb_t ready_cb_;

    ptr<axprt_crypt> x_;
};

#endif
