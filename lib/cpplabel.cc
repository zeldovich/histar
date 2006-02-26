extern "C" {
#include <inc/lib.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <new>

label::label(level_t def) : dynamic_(true)
{
    ul_.ul_size = 0;
    ul_.ul_nent = 0;
    ul_.ul_default = def;
    ul_.ul_ent = 0;
}

label::label(uint64_t *ents, size_t size) : dynamic_(false)
{
    ul_.ul_size = size;
    ul_.ul_nent = 0;
    ul_.ul_default = LB_LEVEL_UNDEF;
    ul_.ul_ent = ents;
}

label::~label()
{
    if (dynamic_ && ul_.ul_ent)
	free(ul_.ul_ent);
}

uint64_t *
label::slot_find(uint64_t handle)
{
    for (uint64_t i = 0; i < ul_.ul_nent; i++)
	if (LB_HANDLE(ul_.ul_ent[i]) == handle)
	    return &ul_.ul_ent[i];
    return 0;
}

uint64_t *
label::slot_grow(uint64_t handle)
{
    for (uint64_t i = 0; i < ul_.ul_nent; i++)
	if (LB_LEVEL(ul_.ul_ent[i]) == ul_.ul_default)
	    return &ul_.ul_ent[i];

    uint64_t n = ul_.ul_nent;
    if (n >= ul_.ul_size)
	grow();

    ul_.ul_nent++;
    ul_.ul_ent[n] = LB_CODE(handle, ul_.ul_default);
    return &ul_.ul_ent[n];
}

uint64_t *
label::slot_alloc(uint64_t handle)
{
    return slot_find(handle) ? : slot_grow(handle);
}

void
label::grow()
{
    if (!dynamic_)
	throw basic_exception("label::grow: statically allocated");

    uint64_t newsize = MAX(ul_.ul_size, 8UL) * 2;
    uint64_t *newent = (uint64_t *) realloc(ul_.ul_ent, newsize);
    if (newent == 0)
	throw std::bad_alloc();

    ul_.ul_ent = newent;
    ul_.ul_size = newsize;
}

level_t
label::get(uint64_t handle)
{
    uint64_t *s = slot_find(handle);
    return s ? LB_LEVEL(*s) : ul_.ul_default;
}

void
label::set(uint64_t handle, level_t level)
{
    uint64_t *s = slot_alloc(handle);
    *s = LB_CODE(handle, level);
}
