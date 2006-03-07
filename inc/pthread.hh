#ifndef JOS_INC_PTHREAD_HH
#define JOS_INC_PTHREAD_HH

extern "C" {
#include <inc/pthread.h>
}

class scoped_pthread_lock {
public:
    scoped_pthread_lock(pthread_mutex_t *mu) : mu_(mu) {
	pthread_mutex_lock(mu_);
    }

    ~scoped_pthread_lock() {
	pthread_mutex_unlock(mu_);
    }

private:
    pthread_mutex_t *mu_;
};

#endif
