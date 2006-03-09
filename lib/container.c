#include <inc/lib.h>
#include <inc/error.h>
#include <inc/syscall.h>
#include <string.h>

int64_t
container_find(uint64_t rc, kobject_type_t reqtype, const char *reqname)
{
    int64_t nslots = sys_container_nslots(rc);
    if (nslots < 0)
	return nslots;

    for (int64_t i = 0; i < nslots; i++) {
	int64_t id = sys_container_get_slot_id(rc, i);
	if (id < 0)
	    continue;

	struct cobj_ref ko = COBJ(rc, id);
	int t = sys_obj_get_type(ko);
	if (t < 0)
	    return t;

	kobject_type_t type = t;
	if (reqtype != kobj_any && reqtype != type)
	    continue;

	char name[KOBJ_NAME_LEN];
	int r = sys_obj_get_name(ko, &name[0]);
	if (r < 0)
	    continue;
	if (reqname && strcmp(&name[0], reqname))
	    continue;

	return id;
    }

    return -E_NOT_FOUND;
}
