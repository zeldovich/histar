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

    if (ac < 4) {
	printf("Usage: %s host-pk call-ct binary [args ...]\n", av[0]);
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

    guardcall_arg arg;
    guardcall_res res;

    arg.parent_ct = call_ct;
    arg.elf_pn = av[3];
    arg.args.setsize(ac - 3);
    for (int i = 3; i < ac; i++)
	arg.args[i - 3].s = av[i];

    fs_inode ino;
    error_check(fs_namei(arg.elf_pn, &ino));
    str bindata = segment_to_str(ino.obj);
    sha1_hash(arg.sha1sum.base(), bindata.cstr(), bindata.len());

    /* Create a remote tainted container */
    dj_message_endpoint guardcall_ep;
    guardcall_ep.set_type(EP_GATE);
    guardcall_ep.ep_gate->msg_ct = call_ct;
    guardcall_ep.ep_gate->gate.gate_ct = 0;
    guardcall_ep.ep_gate->gate.gate_id = GSPEC_GUARDCALL;

    dj_delivery_code c;
    dj_autorpc remote_ar(&gs, 5, k, djcache);
    c = remote_ar.call(guardcall_ep, arg, res);
    if (c != DELIVERY_DONE)
	fatal << "error calling guardcall: code " << c << "\n";
    if (!res.ok)
	fatal << "guardcall: not ok\n";

    warn << "spawned in container " << res.resok->spawn_ct << "\n";
}
