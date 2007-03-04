extern "C" {
#include <inc/syscall.h>
#include <inc/lib.h>
}

#include <inc/labelutil.hh>
#include <dj/djops.hh>
#include <dj/gatesender.hh>
#include <dj/djsrpc.hh>
#include <dj/djautorpc.hh>
#include <dj/djutil.hh>
#include <dj/hsutil.hh>
#include <dj/miscx.h>

int
main(int ac, char **av)
{
    dj_global_cache djcache;

    if (ac != 3) {
	printf("Usage: %s host-pk call-ct\n", av[0]);
	exit(-1);
    }

    gate_sender gs;

    str pkstr(av[1]);
    dj_pubkey k;
    if (pkstr == "0") {
	k = gs.hostkey();
    } else {
	ptr<sfspub> sfspub = sfscrypt.alloc(pkstr, SFS_VERIFY | SFS_ENCRYPT);
	assert(sfspub);
	k = sfspub2dj(sfspub);
    }

    uint64_t call_ct = atoi(av[2]);

    /* First, create a taint category & local container for gcat mapping */
    uint64_t key_t = handle_alloc();

    label ct_label(1);
    /* XXX we need a better way of giving privileges to the local
     * exporter that can't yet be named by a global category..
     */
    //ct_label.set(key_t, 3);

    int64_t local_ct = sys_container_alloc(start_env->proc_container,
					   ct_label.to_ulabel(), "foo",
					   0, CT_QUOTA_INF);
    error_check(local_ct);
    warn << "eprocd-launch: taint " << key_t << " mapct " << local_ct << "\n";

    /* Now, create a mapping & delegation onto the remote machine */
    label map_grant(3);
    //map_grant.set(key_t, LB_LEVEL_STAR);

    dj_cat_mapping lcm, rcm;
    dj_stmt_signed delegation;

    dj_map_and_delegate(key_t, false,
			map_grant, map_grant,
			local_ct, call_ct, k,
			&gs, djcache,
			&lcm, &rcm, &delegation);
    warn << "eprocd-launch: created mappings & delegations\n";

    /* Finally, launch ssl_eprocd */
    guardcall_arg arg;
    guardcall_res res;

    fs_inode servkey_pem_ino;
    error_check(fs_namei("/bin/servkey.pem", &servkey_pem_ino));
    str keydata = segment_to_str(servkey_pem_ino.obj);

    arg.parent_ct = call_ct;
    arg.elf_pn = "/bin/ssl_eprocd";
    arg.args.setsize(3);
    arg.args[0].s = "ssl_eprocd";
    arg.args[1].s = "0";
    arg.args[2].s = keydata;

    arg.glabel.ents.push_back(lcm.gcat);
    arg.gclear.ents.push_back(lcm.gcat);

    fs_inode ino;
    error_check(fs_namei(arg.elf_pn, &ino));
    str bindata = segment_to_str(ino.obj);
    sha1_hash(arg.sha1sum.base(), bindata.cstr(), bindata.len());

    dj_message_endpoint guardcall_ep;
    guardcall_ep.set_type(EP_GATE);
    guardcall_ep.ep_gate->msg_ct = call_ct;
    guardcall_ep.ep_gate->gate.gate_ct = 0;
    guardcall_ep.ep_gate->gate.gate_id = GSPEC_GUARDCALL;

    label msg_taint(1);
    msg_taint.set(key_t, 3);

    label msg_grant(3);
    msg_grant.set(key_t, LB_LEVEL_STAR);

    label msg_clear(0);
    msg_clear.set(key_t, 3);

    warn << "eprocd-launch: trying to start eprocd...\n";
    dj_delivery_code c;
    dj_autorpc remote_ar(&gs, 5, k, djcache);
    c = remote_ar.call(guardcall_ep, arg, res,
		       &msg_taint, &msg_grant, &msg_clear);
    if (c != DELIVERY_DONE)
	fatal << "error calling guardcall: code " << c << "\n";
    if (!res.ok)
	fatal << "guardcall: not ok\n";

    warn << "spawned in container " << res.resok->spawn_ct << "\n";
}
