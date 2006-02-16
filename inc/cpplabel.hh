#ifndef JOS_INC_CPPLABEL_H
#define JOS_INC_CPPLABEL_H

extern "C" {
#include <inc/lib.h>
#include <inc/label.h>
};

class label {
public:
    label(level_t l);

    void set(uint64_t handle, level_t level);
    level_t get(uint64_t handle);

    struct ulabel *get_ulabel();
    const char *to_string() const;

private:
    struct ulabel *buf_;
};

#endif
