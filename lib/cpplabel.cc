#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <new>

label::label(level_t l)
{
    buf_ = label_alloc();
    if (buf_ == 0)
	throw std::bad_alloc();

    buf_->ul_default = l;
}

void
label::set(uint64_t handle, level_t level)
{
    int r = label_set_level(buf_, handle, level, 1);
    if (r < 0)
	throw error(r, "label_set_level");
}

level_t
label::get(uint64_t handle)
{
    level_t l = label_get_level(buf_, handle);
    return l;
}

struct ulabel *
label::get_ulabel()
{
    return buf_;
}

const char *
label::to_string() const
{
    return label_to_string(buf_);
}
