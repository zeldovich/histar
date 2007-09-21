extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/error.h>
#include <stdio.h>
}

#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <dj/checkpoint.hh>

void
checkpoint_update()
{
    segment_unmap_flush();

    int64_t ct_id = sys_container_alloc(start_env->proc_container, 0,
					"as checkpoint", 0, CT_QUOTA_INF);
    error_check(ct_id);

    enum { uas_nent = 64 };
    struct u_segment_mapping uas_ents[uas_nent];
    struct u_address_space uas;
    uas.size = uas_nent;
    uas.ents = &uas_ents[0];

    cobj_ref cur_as;
    error_check(sys_self_get_as(&cur_as));
    error_check(sys_as_get(cur_as, &uas));

    for (uint32_t i = 0; i < uas.nent; i++) {
	struct u_segment_mapping *usm = &uas.ents[i];
	if (!(usm->flags & SEGMAP_WRITE) || usm->segment.container != start_env->proc_container)
	    continue;

	char namebuf[KOBJ_NAME_LEN];
	error_check(sys_obj_get_name(usm->segment, &namebuf[0]));
	int64_t nid = sys_segment_copy(usm->segment, ct_id, 0, &namebuf[0]);
	if (nid == -E_LABEL) {
	    usm->flags = 0;
	    continue;
	}
	error_check(nid);

	usm->segment = COBJ(ct_id, nid);
    }

    int64_t as_id = sys_as_create(ct_id, 0, "checkpoint as");
    error_check(as_id);
    cobj_ref new_as = COBJ(ct_id, as_id);

    error_check(sys_as_set(new_as, &uas));

    if (start_env->taint_cow_as.container)
	sys_obj_unref(COBJ(start_env->proc_container, start_env->taint_cow_as.container));
    start_env->taint_cow_as = new_as;
}
