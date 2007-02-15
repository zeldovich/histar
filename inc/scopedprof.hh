#ifndef JOS_INC_PROF_HH
#define JOS_INC_PROF_HH

extern "C" {
#include <inc/prof.h>
}

class scoped_prof {
public:
    scoped_prof(void);
    ~scoped_prof(void);

private:
    void *func_addr_;
    uint64_t start_;
};

#endif
