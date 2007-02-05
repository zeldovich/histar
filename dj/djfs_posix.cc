#include <crypt.h>
#include <dj/dis.hh>
#include <dj/djfs.h>

class posixfs : public djcallexec {
 public:
    posixfs(djprot::call_reply_cb cb) : cb_(cb) {}

    virtual void start(const dj_gatename &gate, const djcall_args &args) {
	djcall_args ra;

	djfs_request req;
	djfs_reply res;
	if (!str2xdr(req, args.data)) {
	    cb_(REPLY_SYSERR, ra);
	    return;
	}

	res.set_err(0);
	res.d->set_op(req.op);

	switch (req.op) {
	case DJFS_READDIR:
	    warn << "readdir for " << *req.pn << "\n";
	    res.d->ents->setsize(2);
	    (*res.d->ents)[0] = "Hello";
	    (*res.d->ents)[1] = "world";
	    break;

	default:
	    cb_(REPLY_SYSERR, ra);
	    return;
	}

	ra.taint = args.taint;
	ra.grant = args.grant;
	ra.data = xdr2str(res);
	cb_(REPLY_DONE, ra);
    }

    virtual void abort() {
	djcall_args ra;
	cb_(REPLY_ABORTED, ra);
    }

 private:
    djprot::call_reply_cb cb_;
};

ptr<djcallexec>
dj_posixfs_exec(djprot::call_reply_cb cb)
{
    return New refcounted<posixfs>(cb);
}
