#ifndef JOS_INC_JTHREAD_HH
#define JOS_INC_JTHREAD_HH

extern "C" {
#include <inc/jthread.h>
}

class scoped_jthread_lock {
public:
    scoped_jthread_lock(jthread_mutex_t *mu) : mu_(mu) {
	jthread_mutex_lock(mu_);
	held_ = true;
    }

    ~scoped_jthread_lock() {
	release();
    }

    void release() {
	if (held_) {
	    jthread_mutex_unlock(mu_);
	    held_ = false;
	}
    }

private:
    jthread_mutex_t *mu_;
    bool held_;
};

class scoped_jthread_trylock {
public:
    scoped_jthread_trylock(jthread_mutex_t *mu) : mu_(mu) {
	acquired_ = jthread_mutex_trylock(mu_);
    }

    bool acquired() {
	return (acquired_ == 0) ? true : false;
    }

    ~scoped_jthread_trylock() {
	if (acquired_ == 0)
	    jthread_mutex_unlock(mu_);
    }

private:
    jthread_mutex_t *mu_;
    int acquired_;
};

#endif
