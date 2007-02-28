#include <crypt.h>
#include <dj/cryptconn.hh>
#include <dj/djkey.hh>

static void
keygen(sfs_kmsg *kmsg)
{
    rnd.getbytes(kmsg->kcs_share.base(), kmsg->kcs_share.size());
    rnd.getbytes(kmsg->ksc_share.base(), kmsg->ksc_share.size());
}

static void
keyset(ptr<axprt_crypt> x, const sfs_kmsg &local, const sfs_kmsg &remote)
{
    strbuf sendkey;
    sendkey << str(local.kcs_share.base(), local.kcs_share.size());
    sendkey << str(remote.ksc_share.base(), remote.ksc_share.size());

    strbuf recvkey;
    recvkey << str(remote.kcs_share.base(), remote.kcs_share.size());
    recvkey << str(local.ksc_share.base(), local.ksc_share.size());

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
    keygen(&local_kmsg_);

    dj_stmt_signed local_ss;
    local_ss.stmt.set_type(STMT_KEY_SETUP);
    local_ss.stmt.keysetup->sender = p_->pubkey();
    local_ss.stmt.keysetup->to = remote_;

    ptr<sfspub> remotekey = sfscrypt.alloc(remote_, SFS_ENCRYPT);
    if (!remotekey) {
	warn << "crypt_conn: cannot alloc remote key\n";
	die(crypt_cannot_connect);
	return;
    }

    if (!remotekey->encrypt(&local_ss.stmt.keysetup->kmsg, xdr2str(local_kmsg_))) {
	warn << "crypt_conn: cannot encrypt\n";
	die(crypt_cannot_connect);
	return;
    }

    p_->sign_statement(&local_ss);
    str s = xdr2str(local_ss);
    x_->send(s.cstr(), s.len(), 0);
}

void
crypt_conn::key_recv(const char *buf, ssize_t len, const sockaddr*)
{
    if (!buf) {
	die(crypt_cannot_connect);
	return;
    }

    dj_stmt_signed remote_ss;
    if (!buf2xdr(remote_ss, buf, len)) {
	warn << "crypt_conn: cannot unmarshal statement\n";
	die(crypt_cannot_connect);
	return;
    }

    if (!verify_stmt(remote_ss) ||
	remote_ss.stmt.type != STMT_KEY_SETUP ||
	remote_ss.stmt.keysetup->to != p_->pubkey())
    {
	warn << "crypt_conn: bad remote_ss\n";
	die(crypt_cannot_connect);
	return;
    }

    str remote_kmsg_ptext;
    if (!p_->privkey()->decrypt(remote_ss.stmt.keysetup->kmsg,
				&remote_kmsg_ptext, sizeof(sfs_kmsg))) {
	warn << "crypt_conn: cannot decrypt\n";
	die(crypt_cannot_connect);
	return;
    }

    sfs_kmsg remote_kmsg;
    if (!str2xdr(remote_kmsg, remote_kmsg_ptext)) {
	warn << "crypt_conn: cannot unmarshal kmsg\n";
	die(crypt_cannot_connect);
	return;
    }

    if (initiate_) {
	if (remote_ss.stmt.keysetup->sender != remote_) {
	    warn << "crypt_conn: remote key mismatch\n";
	    die(crypt_cannot_connect);
	    return;
	}
    } else {
	remote_ = remote_ss.stmt.keysetup->sender;
	key_send();
    }

    keyset(x_, local_kmsg_, remote_kmsg);
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
