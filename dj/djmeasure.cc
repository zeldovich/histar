extern "C" {
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/bipipe.h>
#include <inc/labelutil.h>
#include <zlib.h>
}

#include <inc/errno.hh>


#include <crypt.h>
#include <sfscrypt.h>
#include <dj/djprotx.h>
#include <dj/djkey.hh>
#include <dj/gatesender.hh>
#include <dj/djsrpc.hh>
#include <dj/djrpcx.h>

enum { crypto_rounds = 100 };
enum { echo_rounds   = 100 };

static int
compressed_size(str m)
{
    unsigned long buflen = m.len() * 2 + 12;
    void *buf = malloc(buflen);
    if (!buf)
	printf("compressed_size: cannot malloc\n");
    int r = compress((Bytef*) buf, &buflen, (Bytef*) m.cstr(), m.len());
    if (r != Z_OK)
	printf("compressed_size: cannot compress: %d\n", r);
    free(buf);
    return buflen;
}

static void
print_size(const char *descr, str m)
{
    printf("  %4d %4d %s\n", m.len(), compressed_size(m), descr);
}

int
main (int ac, char **av)
{
    printf("generating randomness.. ");
    fflush(stdout);
    random_init();
    printf("done.\n");

    int64_t start;

    ptr<sfspriv> sk0(sfscrypt.gen(SFS_RABIN, 0, SFS_SIGN | SFS_VERIFY |
				 SFS_ENCRYPT | SFS_DECRYPT));
    dj_pubkey pk0;
    sk0->export_pubkey(&pk0);        

    ptr<sfspriv> sk1(sfscrypt.gen(SFS_RABIN, 0, SFS_SIGN | SFS_VERIFY |
				 SFS_ENCRYPT | SFS_DECRYPT));
    dj_pubkey pk1;
    sk1->export_pubkey(&pk1);        
    
    struct dj_gcat meow;
    meow.key = pk0;
    meow.id = handle_alloc();
    meow.integrity = 1;

    struct dj_cat_mapping mapping;
    mapping.gcat = meow;
    mapping.lcat = handle_alloc();
    mapping.res_ct = handle_alloc();
    mapping.res_gt = handle_alloc();

    struct dj_delegation cat_delegation;
    cat_delegation.a.set_type(ENT_PUBKEY);
    *cat_delegation.a.key = pk1;
    cat_delegation.b.set_type(ENT_GCAT);
    *cat_delegation.b.gcat = meow;    
    *cat_delegation.via.alloc() = pk0;
    cat_delegation.from_ts = rand();
    cat_delegation.until_ts = rand();

    struct dj_stmt_signed signed_cat_delegation;
    signed_cat_delegation.stmt.set_type(STMT_DELEGATION);
    *signed_cat_delegation.stmt.delegation = cat_delegation;
    str goo = xdr2str(cat_delegation);
    start = sys_clock_nsec();
    for (int i = 0; i < crypto_rounds; i++)
	sk1->sign(&signed_cat_delegation.sign, goo);
    int64_t sign_cat_time = sys_clock_nsec() - start;
    
    struct dj_address addr;
    addr.ip = rand();
    addr.port = rand();

    struct dj_delegation addr_delegation;
    addr_delegation.a.set_type(ENT_ADDRESS);
    *addr_delegation.a.addr = addr;
    addr_delegation.b.set_type(ENT_PUBKEY);
    *addr_delegation.b.key = pk0;    
    cat_delegation.from_ts = rand();
    cat_delegation.until_ts = rand();
    
    struct dj_stmt_signed signed_addr_delegation;
    signed_addr_delegation.stmt.set_type(STMT_DELEGATION);
    *signed_addr_delegation.stmt.delegation = addr_delegation;
    goo = xdr2str(addr_delegation);
    start = sys_clock_nsec();
    for (int i = 0; i < crypto_rounds; i++)
	sk0->sign(&signed_addr_delegation.sign, goo);
    int64_t sign_addr_time = sys_clock_nsec() - start;

    struct dj_message empty_msg;
    memset(&empty_msg, 0, sizeof(empty_msg));
    empty_msg.to = pk1;
    empty_msg.target.set_type(EP_DELEGATOR);
    empty_msg.taint.ents.setsize(0);
    empty_msg.glabel.ents.setsize(0);
    empty_msg.gclear.ents.setsize(0);
    empty_msg.catmap.ents.setsize(0);
    empty_msg.dset.ents.setsize(0);
    empty_msg.msg.setsize(0);

    struct dj_ep_segment seg;
    seg.seg_ct = handle_alloc();
    seg.seg_id = handle_alloc();

    struct dj_message simple_msg0;
    memset(&simple_msg0, 0, sizeof(simple_msg0));
    simple_msg0.to = pk1;
    simple_msg0.target.set_type(EP_SEGMENT);
    *simple_msg0.target.ep_segment = seg;
    simple_msg0.taint.ents.setsize(1);
    simple_msg0.taint.ents.push_back(meow);
    simple_msg0.glabel.ents.setsize(0);
    simple_msg0.gclear.ents.setsize(0);
    simple_msg0.catmap.ents.setsize(0);
    simple_msg0.catmap.ents.push_back(mapping);
    simple_msg0.dset.ents.setsize(0);
    rpc_bytes<2147483647ul> s;
    xdr2bytes(s, signed_cat_delegation);
    simple_msg0.dset.ents.push_back(s);
    simple_msg0.msg.setsize(0);

    dj_call_msg callmsg;
    callmsg.return_host = pk0;
    callmsg.return_ep.set_type(EP_SEGMENT);
    *callmsg.return_ep.ep_segment = seg;
    callmsg.return_cm = simple_msg0.catmap;
    callmsg.return_ds = simple_msg0.dset;

    struct dj_message rpc_msg0;
    memset(&rpc_msg0, 0, sizeof(rpc_msg0));
    rpc_msg0.to = pk1;
    rpc_msg0.target.set_type(EP_SEGMENT);
    *rpc_msg0.target.ep_segment = seg;
    rpc_msg0.taint.ents.setsize(1);
    rpc_msg0.taint.ents.push_back(meow);
    rpc_msg0.glabel.ents.setsize(0);
    rpc_msg0.gclear.ents.setsize(0);
    rpc_msg0.catmap.ents.setsize(0);
    rpc_msg0.catmap.ents.push_back(mapping);
    rpc_msg0.dset.ents.setsize(0);

    xdr2bytes(s, signed_cat_delegation);
    rpc_msg0.dset.ents.push_back(s);
    rpc_msg0.msg = xdr2str(callmsg);

    printf("size:\n");
    printf(" plain zlib what\n");
    print_size("dj_pubkey", xdr2str(pk0));
    print_size("dj_gcat (pk0)", xdr2str(meow));
    print_size("dj_delegation (pk0 says pk1 speaks for gcat)",
		xdr2str(cat_delegation));
    print_size("dj_delegation (pk0 says pk1 speaks for gcat) signed",
		xdr2str(signed_cat_delegation));
    print_size("dj_delegation (pk0 says addr speaks for pk0)",
		xdr2str(addr_delegation));
    print_size("dj_delegation (pk0 says addr speaks for pk0) signed",
		xdr2str(signed_addr_delegation));
    print_size("dj_cat_mapping", xdr2str(mapping));
    print_size("dj_message (to pk1)", xdr2str(empty_msg));
    print_size("dj_message (to pk1, seg slot, 1 taint, 1 delegation, 1 map)",
		xdr2str(simple_msg0));

    start = sys_clock_nsec();
    for (int i = 0; i < crypto_rounds; i++)
	verify_stmt(signed_cat_delegation);
    int64_t verify_cat_time = sys_clock_nsec() - start;

    start = sys_clock_nsec();
    for (int i = 0; i < crypto_rounds; i++)
	verify_stmt(signed_addr_delegation);
    int64_t verify_addr_time = sys_clock_nsec() - start;

    printf("\ntime [%d rounds of everything]:\n", crypto_rounds);
    printf(" sign:\n");
    printf("  dj_delegation (pk0 says pk1 speaks for gcat) %lu\n", 
	    sign_cat_time);
    printf("  dj_delegation (pk0 says addr speaks for pk0) %lu\n",
	    sign_addr_time);
    printf(" verify:\n");
    printf("  dj_delegation (pk0 says pk1 speaks for gcat) %lu\n", 
	    verify_cat_time);
    printf("  dj_delegation (pk0 says addr speaks for pk0) %lu\n",
	    verify_addr_time);

    if (ac == 4) {
	// usage: djmeasure host-pk call-ct djechod-gate
        ptr<sfspub> sfspub = sfscrypt.alloc(av[1], SFS_VERIFY | SFS_ENCRYPT);
	assert(sfspub);
	
	dj_message m;
	m.to = sfspub2dj(sfspub);
	m.target.set_type(EP_GATE);
	m.target.ep_gate->msg_ct = strtoll(av[2], 0, 0);
	m.target.ep_gate->gate <<= av[3];
	m.dset.ents.setsize(0);
	m.catmap.ents.setsize(0);
	m.taint.ents.setsize(0);
	m.glabel.ents.setsize(0);
	m.gclear.ents.setsize(0);
	m.msg.setsize(0);
	
	gate_sender gs;
	struct dj_delegation_set dset;
	dset.ents.setsize(0);
	struct dj_catmap cm;
	cm.ents.setsize(0);
	str calldata = "";
	struct dj_message dm;
	
	dj_delivery_code c = DELIVERY_DONE;
	start = sys_clock_nsec();
	for (int i = 0; i < echo_rounds && c == DELIVERY_DONE; i++)
	    c = dj_rpc_call(&gs, 2, dset, cm, m, calldata, &dm, 0, 0, 0);
	if (c != DELIVERY_DONE)
	    printf("\nunable to measure echo rpc: %d\n", c);
	else
	    printf("\ntime for %u djechos: %lu\n", echo_rounds, 
		   sys_clock_nsec() - start);
    }	

    return 0;
}
