extern "C" {
#include <inc/lib.h>
#include <inc/fs.h>
#include <stdio.h>
}

#include <crypt.h>
#include <inc/labelutil.hh>
#include <dj/djsrpc.hh>
#include <dj/djfs.h>

bool
fs_service(const dj_message &m, const str &s, dj_rpc_reply *r)
{
    djfs_request arg;
    djfs_reply res;

    if (!str2xdr(arg, s)) {
	warn << "fs_service: cannot unmarshal\n";
	return false;
    }

    try {
	res.set_err(0);
	res.d->set_op(arg.op);

	fs_inode ino;
	uint64_t len;

	switch (arg.op) {
	case DJFS_READ:
	    error_check(fs_namei(arg.read->pn, &ino));
	    error_check(fs_getsize(ino, &len));

	    res.d->read->data.setsize(len);
	    error_check(fs_pread(ino, res.d->read->data.base(), len, 0));
	    break;

	case DJFS_WRITE:
	    error_check(fs_namei(arg.write->pn, &ino));
	    len = arg.write->data.size();
	    error_check(fs_resize(ino, len));
	    error_check(fs_pwrite(ino, arg.write->data.base(), len, 0));
	    break;

	default:
	    throw basic_exception("unsupported op %d", arg.op);
	}
    } catch (std::exception &e) {
	warn << "fs_service: " << e.what() << "\n";
	res.set_err(-1);
    }

    r->msg.msg = xdr2str(res);
    return true;
}

static void
gate_entry(uint64_t arg, gate_call_data *gcd, gatesrv_return *r)
{
    dj_rpc_srv(fs_service, gcd, r);
}

int
main(int ac, char **av)
{
    label tl;
    thread_cur_label(&tl);
    tl.set(start_env->process_grant, 1);

    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "djfsd";
    gd.func_ = &gate_entry;
    gd.label_ = &tl;

    cobj_ref g = gate_create(&gd);
    printf("djfsd: gate %lu.%lu\n", g.container, g.object);

    for (;;)
	sleep(60);
}
