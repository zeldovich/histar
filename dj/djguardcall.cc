extern "C" {
#include <inc/syscall.h>
#include <inc/stdio.h>
}

#include <async.h>
#include <inc/spawn.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <dj/djgatesrv.hh>
#include <dj/djsrpc.hh>
#include <dj/djops.hh>
#include <dj/djlabel.hh>
#include <dj/reqcontext.hh>
#include <dj/miscx.h>

static bool
guardcall_service(const dj_message &m, const str &s, dj_rpc_reply *r)
{
    guardcall_arg arg;
    guardcall_res res;

    res.set_ok(false);

    try {
	if (!str2xdr(arg, s)) {
	    warn << "guardcall: str2xdr\n";
	    goto out;
	}

	dj_catmap_indexed cmi(m.catmap);
	label lt, lg, lc;
	djlabel_to_label(cmi, m.taint,  &lt, label_taint);
	djlabel_to_label(cmi, m.glabel, &lg, label_owner);
	djlabel_to_label(cmi, m.gclear, &lc, label_clear);

	label lng, lnc;
	lt.merge(&lg, &lng, label::min, label::leq_starlo);
	lt.merge(&lc, &lnc, label::max, label::leq_starlo);

	verify_label_reqctx ctx(lng, lnc);
	if (!ctx.can_rw(COBJ(arg.parent_ct, arg.parent_ct))) {
	    warn << "guardcall: no permission\n";
	    goto out;
	}

	label spawn_cs, spawn_ds, spawn_dr;
	djlabel_to_label(cmi, arg.taint,  &spawn_cs, label_taint);
	djlabel_to_label(cmi, arg.glabel, &spawn_ds, label_owner);
	djlabel_to_label(cmi, arg.gclear, &spawn_dr, label_clear);

	error_check(lng.compare(&spawn_cs, label::leq_starlo));
	error_check(spawn_cs.compare(&lnc, label::leq_starhi));

	error_check(lng.compare(&spawn_ds, label::leq_starlo));
	error_check(spawn_dr.compare(&lnc, label::leq_starlo));

	/*
	 * This can potentially allow the caller to use djguardcall's
	 * privileges for the namei call.  Not a big deal since it's
	 * read access and djguardcall should have no interesting
	 * privileges.
	 */
	fs_inode elf_ino;
	error_check(fs_namei(arg.elf_pn, &elf_ino));

	if (!ctx.can_read(elf_ino.obj)) {
	    warn << "guardcall: no permission to read elf binary\n";
	    goto out;
	}

	char elfname[KOBJ_NAME_LEN];
	error_check(sys_obj_get_name(elf_ino.obj, &elfname[0]));
	int64_t elf_copy = sys_segment_copy(elf_ino.obj,
					    start_env->proc_container,
					    0, &elfname[0]);
	error_check(elf_copy);

	elf_ino.obj = COBJ(start_env->proc_container, elf_copy);
	scope_guard<int, cobj_ref> unref(sys_obj_unref, elf_ino.obj);

	void *data_map = 0;
	uint64_t data_len = 0;
	error_check(segment_map(elf_ino.obj, 0, SEGMAP_READ,
				&data_map, &data_len, 0));
	scope_guard2<int, void*, int> unmap(segment_unmap_delayed, data_map, 1);

	char digest[20];
	sha1_hash((void *) &digest[0], data_map, data_len);

	if (memcmp(&digest[0], &arg.sha1sum[0], 20)) {
	    warn << "guardcall: sha1 mismatch\n";
	    goto out;
	}

	const char *argv[20];
	if (arg.args.size() > 20) {
	    warn << "guardcall: too many args\n";
	    goto out;
	}

	for (uint32_t i = 0; i < arg.args.size(); i++)
	    argv[i] = arg.args[i].s;

	spawn_descriptor sd;
	sd.ct_ = arg.parent_ct;
	sd.elf_ino_ = elf_ino;
	sd.fd0_ = 0;
	sd.fd1_ = 1;
	sd.fd2_ = 2;

	sd.cs_ = &spawn_cs;
	sd.ds_ = &spawn_ds;
	sd.dr_ = &spawn_dr;
	sd.co_ = &spawn_cs;

	sd.ac_ = arg.args.size();
	sd.av_ = argv;

	sd.spawn_flags_ = SPAWN_NO_AUTOGRANT;
	child_process cp = spawn(&sd);

	res.set_ok(true);
	res.resok->spawn_ct = cp.container;
    } catch (std::exception &e) {
	warn << "guardcall: " << e.what() << "\n";
    }

 out:
    r->msg.msg = xdr2str(res);
    return true;
}

static void
gate_entry(uint64_t arg, gate_call_data *gcd, gatesrv_return *r)
{
    dj_rpc_srv(guardcall_service, gcd, r);
}

int
main(int ac, char **av)
{
    gatesrv_descriptor gd;
    gd.gate_container_ = start_env->shared_container;
    gd.name_ = "djguardcall";
    gd.func_ = &gate_entry;

    cobj_ref g = gate_create(&gd);
    printf("djguardcall: %ld.%ld\n", g.container, g.object);

    thread_halt();
}
