extern "C" {
#include <inc/lib.h>
#include <inc/error.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <new>

label::label() : dynamic_(true), ul_()
{
    ul_.ul_size = 0;
    ul_.ul_ent = 0;
    ul_.ul_nent = 0;
}

label::label(uint64_t *ents, size_t size) : dynamic_(false), ul_()
{
    ul_.ul_size = size;
    ul_.ul_ent = ents;
    ul_.ul_nent = 0;
}

label::label(const label &o) : dynamic_(true), ul_(o.ul_)
{
    if (o.ul_.ul_ent) {
	size_t sz = ul_.ul_size * sizeof(ul_.ul_ent[0]);
	ul_.ul_ent = (uint64_t *) malloc(sz);
	if (ul_.ul_ent == 0) {
	    fprintf(stderr, "label::label: cannot allocate %zd\n", sz);
	    throw std::bad_alloc();
	}
	memcpy(ul_.ul_ent, o.ul_.ul_ent, sz);
    }
}

label::~label()
{
    if (dynamic_ && ul_.ul_ent)
	free(ul_.ul_ent);

    ul_.ul_ent = (uint64_t *) 0xdeadbeef;
}

void
label::grow()
{
    if (!dynamic_)
	throw basic_exception("label::grow: statically allocated");

    if (ul_.ul_needed > ul_.ul_size) {
	uint64_t newsize = ul_.ul_needed;
	uint64_t newbytes = newsize * sizeof(ul_.ul_ent[0]);
	uint64_t *newent = (uint64_t *) realloc(ul_.ul_ent, newbytes);
	if (newent == 0) {
	    fprintf(stderr, "label::grow: could not realloc %"PRIu64" bytes\n", newbytes);
	    throw std::bad_alloc();
	}

	ul_.ul_ent = newent;
	ul_.ul_size = newsize;
    }
}

bool
label::contains(uint64_t cat) const
{
    for (uint32_t i = 0; i < ul_.ul_nent; i++)
	if (ul_.ul_ent[i] == cat)
	    return true;
    return false;
}

void
label::add(uint64_t cat)
{
    if (contains(cat))
	return;

    for (uint32_t i = 0; i < ul_.ul_nent; i++) {
	if (ul_.ul_ent[i] == 0) {
	    ul_.ul_ent[i] = cat;
	    return;
	}
    }

    if (ul_.ul_nent == ul_.ul_size) {
	ul_.ul_needed = ul_.ul_size * 2;
	grow();
    }

    ul_.ul_ent[ul_.ul_nent] = cat;
    ul_.ul_nent++;
}

void
label::remove(uint64_t cat)
{
    for (uint32_t i = 0; i < ul_.ul_nent; i++)
	if (ul_.ul_ent[i] == cat)
	    ul_.ul_ent[i] = 0;
}

void
label::remove(const label &l)
{
    for (uint32_t i = 0; i < l.ul_.ul_nent; i++)
	if (l.ul_.ul_ent[i])
	    remove(l.ul_.ul_ent[i]);
}

label &
label::operator=(const label &src)
{
    ul_.ul_nent = 0;
    for (uint64_t i = 0; i < src.ul_.ul_nent; i++)
	if (src.ul_.ul_ent[i])
	    add(src.ul_.ul_ent[i]);
    return *this;
}

void
label::from_ulabel(const new_ulabel *src)
{
    ul_.ul_nent = 0;
    for (uint64_t i = 0; i < src->ul_nent; i++)
	if (src->ul_ent[i])
	    add(src->ul_ent[i]);
}

bool
label::can_flow_to(const label &b) const
{
    for (uint64_t i = 0; i < ul_.ul_nent; i++) {
	uint64_t c = ul_.ul_ent[i];

	if (!LB_SECRECY(c))
	    continue;

	if (!b.contains(c))
	    return false;
    }

    for (uint64_t i = 0; i < b.ul_.ul_nent; i++) {
	uint64_t c = b.ul_.ul_ent[i];

	if (LB_SECRECY(c))
	    continue;

	if (!contains(c))
	    return false;
    }

    return true;
}
