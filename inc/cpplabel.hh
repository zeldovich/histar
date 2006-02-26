#ifndef JOS_INC_CPPLABEL_HH
#define JOS_INC_CPPLABEL_HH

extern "C" {
#include <inc/lib.h>
#include <inc/label.h>
};

typedef int (*level_comparator) (level_t, level_t);
typedef level_t (*level_merger) (level_t, level_t, level_comparator);

class label {
public:
    label();
    label(level_t def);
    label(uint64_t *ents, size_t size);
    label(const label &l);
    ~label();

    level_t get(uint64_t handle);
    void set(uint64_t handle, level_t level);

    level_t get_default() { return ul_.ul_default; }
    void set_default(level_t l) { ul_.ul_default = l; }
    void reset(level_t def);

    const char *to_string() const { return label_to_string(&ul_); }
    struct ulabel *to_ulabel() { return &ul_; }

    int compare(label *b, label_comparator cmp);
    void merge(label *b, label *out, level_merger m, level_comparator cmp);
    void merge_with(label *b, level_merger m, level_comparator cmp);

    static int leq_starlo(level_t a, level_t b);
    static int leq_starhi(level_t a, level_t b);
    static int eq(level_t a, level_t b);

    static level_t max(level_t a, level_t b, level_comparator cmp);
    static level_t min(level_t a, level_t b, level_comparator cmp);

private:
    void grow();

    uint64_t *slot_grow(uint64_t handle);
    uint64_t *slot_find(uint64_t handle);
    uint64_t *slot_alloc(uint64_t handle);

    bool dynamic_;
    struct ulabel ul_;
};

#endif
