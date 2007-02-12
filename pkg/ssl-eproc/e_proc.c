/*
 * Software RSA engine, using IPC into another process
 * that keeps our RSA keys.
 */

#include <inc/container.h>
#include <inc/stdio.h>
#include "ssleproc.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>

static int rsa_key_handle;

static EVP_PKEY *
eproc_load_privkey(ENGINE *e, const char *key_id,
		   UI_METHOD *ui_method, void *callback_data)
{
    struct cobj_ref eproc_biseg = *(struct cobj_ref *)key_id;
    int fd = bipipe_fd(eproc_biseg, 0, 0);
    if (fd < 0) {
	cprintf("eproc_load_privkey: unable to open bipipe\n");
	return 0;
    }

    unsigned int pub_len[2];
    unsigned char pub_val[2][256];
    int i;
    for (i = 0; i < 2; i++) {
	int cc = read(fd, &pub_len[i], sizeof(pub_len[i]));
	if (cc != sizeof(pub_len[i]) || pub_len[i] > sizeof(pub_val[i])) {
	    cprintf("eproc_load_privkey: size mismatch %d %ld %ld\n",
		    cc, sizeof(pub_len[i]), pub_len[i]);
	    close(fd);
	    return 0;
	}

	int done = 0;
	while (done != pub_len[i]) {
	    cc = read(fd, &pub_val[i][done], pub_len[i] - done);
	    if (cc <= 0) {
		cprintf("eproc_load_privkey: read %d len %d done %d\n",
			cc, pub_len[i], done);
		close(fd);
		return 0;
	    }
	    done += cc;
	}
    }

    RSA *rsa = RSA_new_method(e);
    RSA_set_ex_data(rsa, rsa_key_handle, (char *) (long) fd);
    rsa->e = BN_bin2bn(pub_val[0], pub_len[0], 0);
    rsa->n = BN_bin2bn(pub_val[1], pub_len[1], 0);

    EVP_PKEY *res = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(res, rsa);

    return res;
}

static int
eproc_rsa_op(int flen, const unsigned char *from,
	     unsigned char *to, RSA *rsa, int padding, int op)
{
    int fd = (long) RSA_get_ex_data(rsa, rsa_key_handle);

    write(fd, &op, sizeof(op));
    write(fd, &flen, sizeof(flen));
    write(fd, from, flen);
    write(fd, &padding, sizeof(padding));

    int tlen;
    read(fd, &tlen, sizeof(tlen));
    read(fd, to, tlen);		/* Way to go, OpenSSL interface! */

    int rval = -1;
    read(fd, &rval, sizeof(rval));
    return rval;
}

static int
eproc_rsa_priv_enc(int flen, const unsigned char *from,
		   unsigned char *to, RSA *rsa, int padding)
{
    return eproc_rsa_op(flen, from, to, rsa, padding, eproc_encrypt);
}

static int
eproc_rsa_priv_dec(int flen, const unsigned char *from,
		   unsigned char *to, RSA *rsa, int padding)
{
    return eproc_rsa_op(flen, from, to, rsa, padding, eproc_decrypt);
}

static RSA_METHOD eproc_rsa = {
    "proc-engine RSA method",
    0,	/* rsa_pub_enc */
    0,	/* rsa_pub_dec */
    eproc_rsa_priv_enc,
    eproc_rsa_priv_dec,
    0,	/* rsa_mod_exp */
    0,	/* bn_mod_exp */
    0,	/* init */
    0,	/* finish */
    0,	/* flags */
    0,	/* app_data */
    0,	/* rsa_sign */
    0,	/* rsa_verify */
    0,	/* rsa_keygen */
};

static int
eproc_init(ENGINE *e)
{
    rsa_key_handle = RSA_get_ex_new_index(0, "proc-engine key handle",
					  0, 0, 0);
    return 1;
}

void
ENGINE_load_proc(void)
{
    ENGINE *e = ENGINE_new();
    if (!e)
	return;

    if (!ENGINE_set_id(e, "proc-engine") ||
	!ENGINE_set_name(e, "RSA process engine") ||
	!ENGINE_set_RSA(e, &eproc_rsa) ||
	!ENGINE_set_init_function(e, eproc_init) ||
	!ENGINE_set_load_privkey_function(e, eproc_load_privkey))
	return;

    ENGINE_add(e);
    ENGINE_free(e);
    ERR_clear_error();
}
