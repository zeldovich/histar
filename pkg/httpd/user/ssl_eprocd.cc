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

static RSA *the_key;
static char *cow_stacktop;

static void __attribute__((noreturn))
handle_client(uint64_t ec, uint64_t eo)
{
    int s = bipipe_fd(COBJ(ec, eo), 1, 0);
    error_check(s);
    
    unsigned char pub_e[256], pub_n[256];
    int pub_e_len, pub_n_len;

    pub_e_len = BN_bn2bin(the_key->e, pub_e);
    pub_n_len = BN_bn2bin(the_key->n, pub_n);

    write(s, &pub_e_len, sizeof(pub_e_len));

    write(s, pub_e, pub_e_len);
    write(s, &pub_n_len, sizeof(pub_n_len));
    write(s, pub_n, pub_n_len);

    uint32_t buflen = 8 * 1024 + 1;	/* Go OpenSSL interfaces? */
    unsigned char *fbuf = (unsigned char *) malloc(buflen);
    unsigned char *tbuf = (unsigned char *) malloc(buflen);

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
	struct ssl_eproc_cow_args *d = (struct ssl_eproc_cow_args *) TLS_GATE_ARGS;
	if (!taint_cow(d->root_ct, COBJ(0, 0)))
	    throw error(-E_UNSPEC, "cow didn't happen?");

	tls_revalidate();
	thread_label_cache_invalidate();

	stack_switch(d->privkey_biseg.container, d->privkey_biseg.object,
		     0, 0,
		     cow_stacktop, (void *) &handle_client);
    
	cprintf("eprocd_cow_entry: still running\n");
	thread_halt();
	
    } catch (std::exception &e) {
	cprintf("eprocd_cow_entry: %s\n", e.what());
	thread_halt();
    }
}

static struct cobj_ref
cow_gate_create(uint64_t ct)
{
    struct thread_entry te;
    memset(&te, 0, sizeof(te));
    te.te_entry = (void *) &eprocd_cow_entry;
    te.te_stack = (char *) tls_stack_top - 8;
    error_check(sys_self_get_as(&te.te_as));

    // XXX should we set a verify label on this gate?
    int64_t gate_id = sys_gate_create(ct, &te, 0, 0, 0, "eproc-cow", 0);
    if (gate_id < 0)
	throw error(gate_id, "sys_gate_create");

    // COW'ed gate call copies mapping and segment
    uint64_t entry_ct = start_env->proc_container;
    void *stackbase = 0;
    uint64_t stackbytes = thread_stack_pages * PGSIZE;
    error_check(segment_map(COBJ(0, 0), 0, SEGMAP_STACK | SEGMAP_RESERVE,
			    &stackbase, &stackbytes, 0));
    scope_guard<int, void *> unmap(segment_unmap, stackbase);
    char *stacktop = ((char *) stackbase) + stackbytes;
	
    struct cobj_ref stackobj;
    void *allocbase = stacktop - (4 * PGSIZE);
    error_check(segment_alloc(entry_ct, (4 * PGSIZE), &stackobj,
			      &allocbase, 0, "COW'ed thread stack"));
    unmap.dismiss();

    cow_stacktop = stacktop;
    
    return COBJ(ct, gate_id);
}

int
main(int ac, char **av)
{

    if (ac < 2) {
	printf("Usage: %s servkey-pem\n", av[0]);
	return -1;
    }
    
    char *pemfile = av[1];
    BIO *in = BIO_new(BIO_s_file_internal());
    assert(in);

    BIO_read_filename(in, pemfile);
    EVP_PKEY *pkey = PEM_read_bio_PrivateKey(in, 0, 0, 0);
    assert(pkey);
    assert(pkey->type == EVP_PKEY_RSA || pkey->type == EVP_PKEY_RSA2);
    the_key = pkey->pkey.rsa;

    cow_gate_create(start_env->shared_container);
    
    return 0;
}
