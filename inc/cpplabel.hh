#ifndef JOS_INC_CPPLABEL_HH
#define JOS_INC_CPPLABEL_HH

extern "C" {
#include <inc/lib.h>
#include <inc/label.h>
};

class label {
 public:
    label();				// dynamic
    label(uint64_t *ents, size_t size);	// non-dynamic
    label(const label &l);		// dynamic
    ~label();

    label &operator=(const label &l);
    void from_ulabel(const new_ulabel *ul);
    void from_string(const char *src);

    void grow();

    bool contains(uint64_t cat) const;
    void add(uint64_t cat);
    void add(const struct new_ulabel *ul);
    void add(const label &l);

    void remove(uint64_t cat);
    void remove(const label &l);

    const char *to_string() const { return label_to_string(&ul_); }
    new_ulabel *to_ulabel() { return &ul_; }
    const new_ulabel *to_ulabel_const() const { return &ul_; }

    //int compare(const label *b, label_comparator cmp) const;
    //void merge(const label *b, label *out, level_merger m, level_comparator cmp) const;
    //void transform(level_changer t, int arg);

 private:
    bool dynamic_;
    new_ulabel ul_;
};

#endif
