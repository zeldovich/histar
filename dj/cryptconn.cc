#include <crypt.h>
#include <dj/cryptconn.hh>
#include <dj/djkey.hh>

static void
keygen(dj_key_setup *ks)
{
    rnd.getbytes(ks->keypart_send.base(), ks->keypart_send.size());
    rnd.getbytes(ks->keypart_recv.base(), ks->keypart_recv.size());
}

static void
keyset(ptr<axprt_crypt> x, const dj_key_setup &local, const dj_key_setup &remote)
{
    strbuf sendkey;
    sendkey << str(local.keypart_send.base(), local.keypart_send.size());
    sendkey << str(remote.keypart_recv.base(), remote.keypart_recv.size());

    strbuf recvkey;
    recvkey << str(remote.keypart_send.base(), remote.keypart_send.size());
    recvkey << str(local.keypart_recv.base(), local.keypart_recv.size());

    x->encrypt(sendkey, recvkey);
}


crypt_conn::crypt_conn(int fd, djprot *p,
		       rcb_t cb, readycb_t ready_cb)
    : initiate_(false), p_(p), cb_(cb), ready_cb_(ready_cb)
{
    x_ = axprt_crypt::alloc(fd);
    x_->setrcb(wrap(this, &crypt_conn::key_recv));
}

crypt_conn::crypt_conn(int fd, dj_pubkey remote, djprot *p,
		       rcb_t cb, readycb_t ready_cb)
    : initiate_(true), remote_(remote), p_(p), cb_(cb), ready_cb_(ready_cb)
{
    x_ = axprt_crypt::alloc(fd);
    x_->setrcb(wrap(this, &crypt_conn::key_recv));

    key_send();
}

void
crypt_conn::key_send()
{
    local_ss_.stmt.set_type(STMT_KEY_SETUP);
    keygen(&*local_ss_.stmt.keysetup);
    local_ss_.stmt.keysetup->host = p_->pubkey();
    p_->sign_statement(&local_ss_);

    ptr<sfspub> remotekey = sfscrypt.alloc(remote_, SFS_ENCRYPT);
    if (!remotekey) {
	warn << "crypt_conn: cannot alloc remote key\n";
	die(crypt_cannot_connect);
	return;
    }

    sfs_ctext local_ctext;
    if (!remotekey->encrypt(&local_ctext, xdr2str(local_ss_))) {
	warn << "crypt_conn: cannot encrypt\n";
	die(crypt_cannot_connect);
	return;
    }

    str s = xdr2str(local_ctext);
    x_->send(s.cstr(), s.len(), 0);
}

void
crypt_conn::key_recv(const char *buf, ssize_t len, const sockaddr*)
{
    if (!buf) {
	die(crypt_cannot_connect);
	return;
    }

    sfs_ctext remote_ctext;
    if (!buf2xdr(remote_ctext, buf, len)) {
	warn << "crypt_conn: cannot unmarshal ctext\n";
	die(crypt_cannot_connect);
	return;
    }

    str remote_ptext;
    if (!p_->privkey()->decrypt(remote_ctext, &remote_ptext)) {
	warn << "crypt_conn: cannot decrypt\n";
	die(crypt_cannot_connect);
	return;
    }

    dj_stmt_signed remote_ss;
    if (!str2xdr(remote_ss, remote_ptext)) {
	warn << "crypt_conn: cannot unmarshal\n";
	die(crypt_cannot_connect);
	return;
    }

    if (!verify_stmt(remote_ss) || remote_ss.stmt.type != STMT_KEY_SETUP) {
	warn << "crypt_conn: bad remote_ss\n";
	die(crypt_cannot_connect);
	return;
    }

    if (initiate_) {
	if (remote_ss.stmt.keysetup->host != remote_) {
	    warn << "crypt_conn: remote key mismatch\n";
	    die(crypt_cannot_connect);
	    return;
	}
    } else {
	remote_ = remote_ss.stmt.keysetup->host;
	key_send();
    }

    keyset(x_, *local_ss_.stmt.keysetup, *remote_ss.stmt.keysetup);
    x_->setrcb(wrap(this, &crypt_conn::data_recv));
    ready_cb_(this, crypt_connected);
}

void
crypt_conn::data_recv(const char *buf, ssize_t len, const sockaddr*)
{
    if (buf == 0) {
	die(crypt_disconnected);
    } else {
	str msg(buf, len);
	cb_(remote_, msg);
    }
}

void
crypt_conn::send(const str &msg)
{
    x_->send(msg.cstr(), msg.len(), 0);
}
