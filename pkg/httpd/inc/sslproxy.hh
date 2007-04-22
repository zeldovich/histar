#ifndef JOS_HTTPD_INC_SSLPROXY_HH
#define JOS_HTTPD_INC_SSLPROXY_HH

extern "C" {
#include <string.h>
#include <machine/atomic.h>
}

enum { 
    ssl_proxy_bipipe_client, 
    ssl_proxy_bipipe_ssld 
};

struct ssl_proxy_client {
 public:
    cobj_ref plain_bipipe_;
    jos_atomic64_t ref_;
};

struct ssl_proxy_descriptor {
 public:
    ssl_proxy_descriptor() :
	base_ct_(0), ssl_ct_(0), 
	sock_fd_(0),
	taint_(0),
	cipher_bipipe_(COBJ(0, 0)), client_seg_(COBJ(0, 0)),
	eproc_started_(0), ssld_started_(0) 
    {
	memset(&eproc_worker_args_, 0, sizeof(eproc_worker_args_));
	memset(&ssld_worker_args_, 0, sizeof(ssld_worker_args_));
    }

    uint64_t base_ct_;
    uint64_t ssl_ct_;

    int sock_fd_;

    uint64_t taint_;

    cobj_ref cipher_bipipe_;
    cobj_ref client_seg_;
    //cobj_ref plain_bipipe_;

    char eproc_started_;
    char ssld_started_;
    thread_args eproc_worker_args_;
    thread_args ssld_worker_args_;
 private:
    ssl_proxy_descriptor(const ssl_proxy_descriptor&);
    ssl_proxy_descriptor &operator=(const ssl_proxy_descriptor&);

};

void ssl_proxy_alloc(cobj_ref ssld_gate, cobj_ref eproc_gate, 
		     uint64_t base_ct, int sock_fd, ssl_proxy_descriptor *d);
void ssl_proxy_cleanup(ssl_proxy_descriptor *d);
void ssl_proxy_loop(ssl_proxy_descriptor *d, char cleanup);
void ssl_proxy_thread(ssl_proxy_descriptor *d, char cleanup);

int ssl_proxy_client_fd(cobj_ref plain_seg);
void ssl_proxy_client_done(cobj_ref plain_seg);

#endif
