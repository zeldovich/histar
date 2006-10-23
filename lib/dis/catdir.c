#include <inc/lib.h>
#include <inc/error.h>

#include <string.h>


#include <inc/dis/catdir.h>


int 
catdir_alloc(uint64_t container, uint64_t size,
	     struct cobj_ref *cobj, struct catdir **va_p,
	     struct ulabel *label, const char *name)
{
    uint64_t bytes = 8 + size * (8 + sizeof(struct global_cat));;
    int r = segment_alloc(container, bytes, cobj, (void **)va_p, label, name);
    if (r < 0)
	return r;
    memset(*va_p, 0, bytes);
    (*va_p)->size = size;
    return r;
}

int 
catdir_map(struct cobj_ref catdir_seg, uint64_t flags,
	   struct catdir **va_p, uint64_t map_opts)
{
    return segment_map(catdir_seg, 0, flags, 
		       (void  **)va_p, 0, map_opts);
}

int 
catdir_insert(struct catdir *cd, uint64_t local, struct global_cat *global)
{
    for (uint32_t i = 0; i < cd->size; i++) {
	if (cd->entry[i].local)
	    continue;

	cd->entry[i].local = local;
	memcpy(&cd->entry[i].global, global, sizeof(cd->entry[i].global));
	return 0;
    }

    return -E_NO_SPACE;
}

int 
catdir_lookup_global(struct catdir *cd, uint64_t local, struct global_cat *global)
{
    for (uint32_t i = 0; i < cd->size; i++) {
	if (!cd->entry[i].local)
	    continue;
	if (cd->entry[i].local == local) {
	    memcpy(global, &cd->entry[i].global, sizeof(*global));
	    return 0;
	}
    }
    
    return -E_NOT_FOUND;
}


int 
catdir_lookup_local(struct catdir *cd, struct global_cat *global, uint64_t *local)
{
    for (uint32_t i = 0; i < cd->size; i++) {
	if (!cd->entry[i].local)
	    continue;
	if (!memcmp(&cd->entry[i].global, global, sizeof(cd->entry[i].global))) {
	    *local = cd->entry[i].local;
	    return 0;
	}
    }

    return -E_NOT_FOUND;
}
