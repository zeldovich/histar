extern "C" {
#include <inc/lib.h>
#include <inc/error.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <new>

label::label() : dynamic_(true)
{
    ul_.ul_size = 0;
    ul_.ul_ent = 0;
    reset(LB_LEVEL_UNDEF);
}

label::label(level_t def) : dynamic_(true)
{
    ul_.ul_size = 0;
    ul_.ul_ent = 0;
    reset(def);
}

label::label(uint64_t *ents, size_t size) : dynamic_(false)
{
    ul_.ul_size = size;
    ul_.ul_ent = ents;
    reset(LB_LEVEL_UNDEF);
}

label::label(const label &o) : dynamic_(true)
{
    ul_ = o.ul_;
    if (o.ul_.ul_ent) {
	size_t sz = ul_.ul_size * sizeof(ul_.ul_ent[0]);
	ul_.ul_ent = (uint64_t *) malloc(sz);
	if (ul_.ul_ent == 0)
	    throw std::bad_alloc();
	memcpy(ul_.ul_ent, o.ul_.ul_ent, sz);
    }
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
    if (n >= ul_.ul_size) {
	ul_.ul_needed = MAX(ul_.ul_size, 8UL);
	grow();
    }

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

    if (ul_.ul_needed) {
	uint64_t newsize = ul_.ul_size + ul_.ul_needed;
	uint64_t newbytes = newsize * sizeof(ul_.ul_ent[0]);
	uint64_t *newent = (uint64_t *) realloc(ul_.ul_ent, newbytes);
	if (newent == 0)
	    throw std::bad_alloc();

	ul_.ul_ent = newent;
	ul_.ul_size = newsize;
    }
}

void
label::reset(level_t def)
{
    ul_.ul_nent = 0;
    ul_.ul_default = def;
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

int
label::compare(label *b, label_comparator cmp)
{
    int r;

    r = cmp(ul_.ul_default, b->ul_.ul_default);
    if (r < 0)
	return r;

    for (uint64_t i = 0; i < ul_.ul_nent; i++) {
	uint64_t h = LB_HANDLE(ul_.ul_ent[i]);
	r = cmp(get(h), b->get(h));
	if (r < 0)
	    return r;
    }

    for (uint64_t i = 0; i < b->ul_.ul_nent; i++) {
	uint64_t h = LB_HANDLE(b->ul_.ul_ent[i]);
	r = cmp(get(h), b->get(h));
	if (r < 0)
	    return r;
    }

    return 0;
}

void
label::merge(label *b, label *out, level_merger m, level_comparator cmp)
{
    out->reset(m(get_default(), b->get_default(), cmp));
    for (uint64_t i = 0; i < ul_.ul_nent; i++) {
	uint64_t h = LB_HANDLE(ul_.ul_ent[i]);
	out->set(h, m(get(h), b->get(h), cmp));
    }

    for (uint64_t i = 0; i < b->ul_.ul_nent; i++) {
	uint64_t h = LB_HANDLE(b->ul_.ul_ent[i]);
	out->set(h, m(get(h), b->get(h), cmp));
    }
}

void
label::merge_with(label *b, level_merger m, level_comparator cmp)
{
    set_default(m(get_default(), b->get_default(), cmp));
    for (uint64_t i = 0; i < b->ul_.ul_nent; i++) {
	uint64_t h = LB_HANDLE(b->ul_.ul_ent[i]);
	set(h, m(get(h), b->get(h), cmp));
    }

    for (uint64_t i = 0; i < ul_.ul_nent; i++) {
	uint64_t h = LB_HANDLE(ul_.ul_ent[i]);
	set(h, m(get(h), b->get(h), cmp));
    }
}

void
label::transform(level_changer t)
{
    set_default(t(get_default()));
    for (uint64_t i = 0; i < ul_.ul_nent; i++) {
	uint64_t h = LB_HANDLE(ul_.ul_ent[i]);
	set(h, t(get(h)));
    }
}

level_t
label::max(level_t a, level_t b, level_comparator leq)
{
    return leq(a, b) == 0 ? b : a;
}

level_t
label::min(level_t a, level_t b, level_comparator leq)
{
    return leq(a, b) == 0 ? a : b;
}

int
label::leq_starlo(level_t a, level_t b)
{
    if (a == LB_LEVEL_STAR)
	return 0;
    if (b == LB_LEVEL_STAR)
	return -E_LABEL;
    if (a <= b)
	return 0;
    return -E_LABEL;
}

int
label::leq_starhi(level_t a, level_t b)
{
    if (b == LB_LEVEL_STAR)
	return 0;
    if (a == LB_LEVEL_STAR)
	return -E_LABEL;
    if (a <= b)
	return 0;
    return -E_LABEL;
}

int
label::eq(level_t a, level_t b)
{
    if (a == b)
	return 0;
    return -E_LABEL;
}

level_t
label::star_to_0(level_t l)
{
    if (l == LB_LEVEL_STAR)
	return 0;
    return l;
}
