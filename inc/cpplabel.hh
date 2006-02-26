#ifndef JOS_INC_CPPLABEL_HH
#define JOS_INC_CPPLABEL_HH

extern "C" {
#include <inc/label.h>
};

class label {
public:
    label(level_t def);
    label(uint64_t *ents, size_t size);
    ~label();

    level_t get(uint64_t handle);
    void set(uint64_t handle, level_t level);

    level_t get_default() { return ul_.ul_default; }
    void set_default(level_t level) { ul_.ul_default = level; }

    const char *to_string() const { return label_to_string(&ul_); }
    struct ulabel *to_ulabel() { return &ul_; }

private:
    void grow();

    uint64_t *slot_grow(uint64_t handle);
    uint64_t *slot_find(uint64_t handle);
    uint64_t *slot_alloc(uint64_t handle);

    bool dynamic_;
    struct ulabel ul_;
};

#endif
