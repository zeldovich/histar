/*
 * Daemon that holds onto an RSA private key and performs
 * encryption/decryption requests with it.
 */

extern "C" {
#include <inc/syscall.h>
#include <inc/memlayout.h>
#include <inc/string.h>
#include <inc/stdio.h>
#include <inc/gateparam.h>
#include <inc/taint.h>
#include <inc/error.h>
#include <inc/stack.h>
#include <inc/bipipe.h>
#include <inc/ssleproc.h>
#include <inc/debug.h>

#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

#include <unistd.h>
}

#include <inc/gatesrv.hh>
#include <inc/gateclnt.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/ssldclnt.hh>

static const char dbg = 0;

static RSA *the_key;
static char *cow_stacktop;

static void __attribute__((noreturn))
handle_client(uintptr_t arg)
{
    debug_cprint(dbg, "opening bipipe...");

    jcomm_ref *comm = (jcomm_ref *)arg;

    // don't worry about extra taint and grant
    int s = bipipe_fd(*comm, 0, 0, 0);
    error_check(s);
    
    unsigned char pub_e[256], pub_n[256];
    int pub_e_len, pub_n_len;

    pub_e_len = BN_bn2bin(the_key->e, pub_e);
    pub_n_len = BN_bn2bin(the_key->n, pub_n);

    debug_cprint(dbg, "writing pub* to bipipe");
    write(s, &pub_e_len, sizeof(pub_e_len));

    write(s, pub_e, pub_e_len);
    write(s, &pub_n_len, sizeof(pub_n_len));
    write(s, pub_n, pub_n_len);

    uint32_t buflen = 8 * 1024 + 1;	/* Go OpenSSL interfaces? */
    unsigned char *fbuf = (unsigned char *) malloc(buflen);
    unsigned char *tbuf = (unsigned char *) malloc(buflen);

    sys_self_set_cflush(1);

    debug_cprint(dbg, "waiting to sign...");
    for (;;) {
	int op, padding, rval;
	unsigned int flen, tlen = 0;

	uint32_t cc = read(s, &op, sizeof(op));
	if (cc != sizeof(op))
	    break;

	cc = read(s, &flen, sizeof(flen));
	if (cc != sizeof(flen) || flen > buflen)
	    break;

	cc = read(s, fbuf, flen);
	if (cc != flen)
	    break;

	cc = read(s, &padding, sizeof(padding));
	if (cc != sizeof(padding))
	    break;

	sys_self_yield();
	if (op == eproc_encrypt) {
	    rval = RSA_private_encrypt(flen, fbuf, tbuf, the_key, padding);
	    if (rval > 0)
		tlen = rval;
	} else if (op == eproc_decrypt) {
	    rval = RSA_private_decrypt(flen, fbuf, tbuf, the_key, padding);
	    if (rval > 0)
		tlen = BN_num_bytes(the_key->n);
	} else {
	    cprintf("Unknown op %d\n", op);
	    break;
	}

	write(s, &tlen, sizeof(tlen));
	write(s, tbuf, tlen);
	write(s, &rval, sizeof(rval));
    }

    free(fbuf);
    free(tbuf);
    close(s);
    thread_halt();
}

static void __attribute__((noreturn))
eprocd_cow_entry(void)
{
    try {
	struct ssl_eproc_cow_args *d =
	    (struct ssl_eproc_cow_args *) &tls_data->tls_gate_args.param_buf[0];
	if (!taint_cow(d->root_ct, COBJ(0, 0)))
	    throw error(-E_UNSPEC, "cow didn't happen?");

	tls_revalidate();
	thread_label_cache_invalidate();

	debug_cprint(dbg, "COWed and ready to handled client...");

	jcomm_ref *comm = (jcomm_ref *)cow_stacktop;
	cow_stacktop -= sizeof(jcomm_ref);
	*comm = d->privkey_comm;
	stack_switch((uintptr_t)comm, 0, 0, 0, 
		     cow_stacktop, (void *) &handle_client);
    
	cprintf("eprocd_cow_entry: still running\n");
	thread_halt();
	
    } catch (std::exception &e) {
	cprintf("eprocd_cow_entry: %s\n", e.what());
	thread_halt();
    }
}

static struct cobj_ref
cow_gate_create(uint64_t ct, uint64_t verify)
{
    struct thread_entry te;
    memset(&te, 0, sizeof(te));
    te.te_entry = (void *) &eprocd_cow_entry;
    te.te_stack = (char *) tls_stack_top - 8;
    error_check(sys_self_get_as(&te.te_as));

    label verify_label(3);
    if (verify)
	verify_label.set(verify, 0);

    int64_t gate_id = sys_gate_create(ct, &te, 0, 0,
				      verify_label.to_ulabel(), "eproc-cow", 0);
    if (gate_id < 0)
	throw error(gate_id, "sys_gate_create");

    // COW'ed gate call copies mapping and segment
    uint64_t entry_ct = start_env->proc_container;

    struct cobj_ref stackobj;
    error_check(segment_alloc(entry_ct, PGSIZE, &stackobj,
			      0, 0, "gate thread stack"));
    scope_guard<int, cobj_ref> s(sys_obj_unref, stackobj);
    
    void *stackbase = 0;
    uint64_t stackbytes = thread_stack_pages * PGSIZE;
    error_check(segment_map(stackobj, 0, SEGMAP_READ | SEGMAP_WRITE |
			    SEGMAP_STACK | SEGMAP_REVERSE_PAGES,
			    &stackbase, &stackbytes, 0));
    char *stacktop = ((char *) stackbase) + stackbytes;
    
    s.dismiss();

    cow_stacktop = stacktop;
    
    return COBJ(ct, gate_id);
}

int
main(int ac, char **av)
{
    if (ac < 3) {
	printf("Usage: %s verify-handle servkey-pem-data\n", av[0]);
	return -1;
    }
    
    uint64_t verify;
    error_check(strtou64(av[1], 0, 10, &verify));

    char *pemdata = av[2];
    int pemlen = strlen(pemdata);
    BIO *in = BIO_new(BIO_s_mem());
    assert(in);
    BIO_write(in, pemdata, pemlen);

    EVP_PKEY *pkey = PEM_read_bio_PrivateKey(in, 0, 0, 0);
    assert(pkey);
    assert(pkey->type == EVP_PKEY_RSA || pkey->type == EVP_PKEY_RSA2);
    the_key = pkey->pkey.rsa;

    cow_gate_create(start_env->shared_container, verify);

    cprintf("eprocd: ready\n");
    
    return 0;
}
