extern "C" {
#include <inc/lib.h>
}

#include <dj/hsutil.hh>
#include <inc/scopeguard.hh>

cobj_ref
str_to_segment(uint64_t container, str data, const char *name)
{
    cobj_ref obj;
    void *base = 0;
    error_check(segment_alloc(container, data.len(), &obj,
			      &base, 0, name));
    memcpy(base, data.cstr(), data.len());
    segment_unmap_delayed(base, 1);
    return obj;
}

str
segment_to_str(cobj_ref seg)
{
    void *base = 0;
    uint64_t len = 0;
    error_check(segment_map(seg, 0, SEGMAP_READ, &base, &len, 0));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, base, 1);
    return str((const char *) base, len);
}
