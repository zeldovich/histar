#ifndef JOS_DJ_HSUTIL_HH
#define JOS_DJ_HSUTIL_HH

extern "C" {
#include <inc/container.h>
}

#include <async.h>

cobj_ref str_to_segment(uint64_t container, str data, const char *name);
str segment_to_str(cobj_ref seg);

#endif
