extern "C" {
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/bipipe.h>
#include <inc/labelutil.h>
}

#include <inc/errno.hh>


#include <crypt.h>
#include <sfscrypt.h>
#include <dj/djprotx.h>
#include <dj/djkey.hh>

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
    *cat_delegation.a.key = pk0;
    cat_delegation.b.set_type(ENT_GCAT);
    *cat_delegation.b.gcat = meow;    
    *cat_delegation.via.alloc() = pk1;
    cat_delegation.from_ts = rand();
    cat_delegation.until_ts = rand();

    struct dj_stmt_signed signed_cat_delegation;
    signed_cat_delegation.stmt.set_type(STMT_DELEGATION);
    *signed_cat_delegation.stmt.delegation = cat_delegation;
    str goo = xdr2str(cat_delegation);
    start = sys_clock_nsec();
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
    *addr_delegation.via.alloc() = pk0;
    cat_delegation.from_ts = rand();
    cat_delegation.until_ts = rand();
    
    struct dj_stmt_signed signed_addr_delegation;
    signed_addr_delegation.stmt.set_type(STMT_DELEGATION);
    *signed_addr_delegation.stmt.delegation = addr_delegation;
    goo = xdr2str(addr_delegation);
    sk0->sign(&signed_addr_delegation.sign, goo);

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
    simple_msg0.dset.ents.setsize(1);
    rpc_bytes<2147483647ul> s;
    xdr2bytes(s, signed_cat_delegation);
    simple_msg0.dset.ents.push_back(s);
    simple_msg0.msg.setsize(0);
    
    printf("size:\n");
    printf(" dj_pubkey %lu\n",
	    xdr2str(pk0).len());
    printf(" dj_delegation (pk1 says pk0 speaks for gcat) %lu\n", 
	    xdr2str(cat_delegation).len());
    printf("  signed %lu\n", xdr2str(signed_cat_delegation).len());
    printf(" dj_delegation (pk0 says addr speaks for pk0) %lu\n", 
	    xdr2str(addr_delegation).len());
    printf("  signed %lu\n", xdr2str(signed_addr_delegation).len());
    printf(" dj_cat_mapping %lu\n", xdr2str(mapping).len());
    printf(" dj_gcat %lu\n", xdr2str(meow).len());
    printf(" dj_message (from, to) %lu\n", xdr2str(empty_msg).len());
    printf(" dj_message (from, to, seg slot, 1 taint, 1 delegation) %lu\n", 
	    xdr2str(simple_msg0).len());

    start = sys_clock_nsec();
    verify_stmt(signed_cat_delegation);
    int64_t verify_cat_time = sys_clock_nsec() - start;

    printf("\ntime:\n");
    printf(" sign:\n");
    printf("  dj_delegation (pk1 says pk0 speaks for gcat) %lu\n", 
	    sign_cat_time);
    printf(" verify:\n");
    printf("  dj_delegation (pk1 says pk0 speaks for gcat) %lu\n", 
	    verify_cat_time);
    
    return 0;
}
