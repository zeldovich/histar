#ifndef JOS_INC_CPPLABEL_HH
#define JOS_INC_CPPLABEL_HH

extern "C" {
#include <inc/lib.h>
#include <inc/label.h>
};

typedef int (*level_comparator) (level_t, level_t);
typedef level_t (*level_merger) (level_t, level_t, level_comparator);
typedef level_t (*level_changer) (level_t, int);

class label {
 public:
    label();				// dynamic
    label(level_t def);			// dynamic
    label(uint64_t *ents, size_t size);	// non-dynamic
    label(const label &l);		// dynamic
    ~label();

    label &operator=(const label &l);
    void from_ulabel(const ulabel *ul);
    void from_string(const char *src);

    void grow();

    level_t get(uint64_t handle) const;
    void set(uint64_t handle, level_t level);

    level_t get_default() const { return ul_.ul_default; }
    void reset(level_t def);

    const char *to_string() const { return label_to_string(&ul_); }
    ulabel *to_ulabel() { return &ul_; }
    const ulabel *to_ulabel_const() const { return &ul_; }

    int compare(const label *b, label_comparator cmp) const;
    void merge(const label *b, label *out, level_merger m, level_comparator cmp) const;
    void transform(level_changer t, int arg);

    static int leq_starlo(level_t a, level_t b);
    static int leq_starhi(level_t a, level_t b);
    static int eq(level_t a, level_t b);

    static level_t max(level_t a, level_t b, level_comparator cmp);
    static level_t min(level_t a, level_t b, level_comparator cmp);

    static level_t star_to(level_t l, int arg);
    static level_t nonstar_to(level_t l, int arg);

 private:
    void set_default(level_t l) { ul_.ul_default = l; }

    uint64_t *slot_grow(uint64_t handle);
    uint64_t *slot_find(uint64_t handle) const;
    uint64_t *slot_alloc(uint64_t handle);

    static level_t string_to_level(const char *str);

    bool dynamic_;
    ulabel ul_;
};

#endif
