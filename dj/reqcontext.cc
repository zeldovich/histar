#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <dj/reqcontext.hh>
#include <dj/djops.hh>

enum { reqctx_debug = 0 };

bool
verify_label_reqctx::can_read(cobj_ref o)
{
    try {
	if (o.container != o.object && !can_read(COBJ(o.container, o.container)))
	    return false;

	label l;
	obj_get_label(o, &l);
	if (reqctx_debug)
	    warn << "can_read(" << o << "): label " << l.to_string() << "\n";
	error_check(l.compare(&vl_, label::leq_starhi));
	return true;
    } catch (std::exception &e) {
	warn << "verify_label_reqctx::can_read " << o
	     << ": " << e.what() << "\n";
	return false;
    }
}

bool
verify_label_reqctx::can_rw(cobj_ref o)
{
    try {
	if (o.container != o.object && !can_read(COBJ(o.container, o.container)))
	    return false;

	label l;
	obj_get_label(o, &l);
	if (reqctx_debug)
	    warn << "can_rw(" << o << "): label " << l.to_string() << ", vl " << vl_.to_string() << "\n";
	error_check(vl_.compare(&l, label::leq_starlo));
	error_check(l.compare(&vl_, label::leq_starhi));
	return true;
    } catch (std::exception &e) {
	warn << "verify_label_reqctx::can_rw " << o
	     << ": " << e.what() << "\n";
	return false;
    }
}

void
verify_label_reqctx::read_seg(cobj_ref o, str *buf)
{
    if (!can_read(o))
	throw basic_exception("vl_reqctx::read_seg: cannot read object");

    void *data_map = 0;
    uint64_t data_len = 0;
    error_check(segment_map(o, 0, SEGMAP_READ, &data_map, &data_len, 0));
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);
    buf->setbuf((const char *) data_map, data_len);
}
