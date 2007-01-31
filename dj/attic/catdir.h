#ifndef JOS_INC_DIS_CATDIR_H
#define JOS_INC_DIS_CATDIR_H

#include <dj/globalcat.h>

struct catdir 
{
    uint64_t size;
    struct {
	uint64_t local;
	struct global_cat global;
    } entry[0];
};

int catdir_alloc(uint64_t container, uint64_t size,
		 struct cobj_ref *cobj, struct catdir **va_p,
		 struct ulabel *label, const char *name);
int catdir_map(struct cobj_ref catdir_seg, uint64_t flags,
	       struct catdir **va_p, uint64_t map_opts);
int catdir_insert(struct catdir *cd, uint64_t local, struct global_cat *global);
int catdir_lookup_global(struct catdir *cd, uint64_t local, struct global_cat *global);
int catdir_lookup_local(struct catdir *cd, struct global_cat *global, uint64_t *local);

#endif
