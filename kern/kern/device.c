#include <kern/arch.h>
#include <kern/device.h>
#include <kern/kobj.h>
#include <inc/error.h>

void
device_swapin(struct Device *dv)
{
    LIST_INIT(&dv->dv_segmap_list);
}

int
device_set_as(const struct Device *const_dv, struct cobj_ref asref)
{
    // XXX what happens if the AS is unrefed?
    const struct kobject *const_ko;
    int r = cobj_get(asref, kobj_address_space, &const_ko, iflow_rw);
    if (r < 0)
	return r;

    if (const_ko->hdr.ko_flags & KOBJ_DEVICE_DEPENDS)
	return -E_INVAL;

    struct Device *dv = &kobject_dirty(&const_dv->dv_ko)->dv;
    if (dv->dv_as) {
	dv->dv_ko.ko_flags &= ~KOBJ_DEVICE_DEPENDS;
	kobject_unpin_hdr(&dv->dv_as->as_ko);
    }

    struct kobject *ko = kobject_dirty(&const_ko->hdr);
    ko->hdr.ko_flags |= KOBJ_DEVICE_DEPENDS;
    kobject_pin_hdr(&ko->hdr);

    dv->dv_as = &ko->as;
    as_invalidate(dv->dv_as);

    // XXX IOMMU something or other

    return 0;
}
