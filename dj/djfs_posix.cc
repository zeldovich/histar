extern "C" {
#include <dirent.h>
#include <sys/types.h>
}

#include <inc/scopeguard.hh>
#include <exception>
#include <crypt.h>
#include <dj/dis.hh>
#include <dj/djfs.h>

class errno_exception {
 public:
    errno_exception(int e) : err_(e) {}
    int err() { return err_; }
 private:
    int err_;
};

static void
errcheck(bool expr)
{
    if (expr)
	throw errno_exception(errno ? errno : ENOTTY);
}

class posixfs : public djcallexec {
 public:
    posixfs(djprot::call_reply_cb cb) : cb_(cb) {}

    virtual void start(const dj_gatename &gate, const djcall_args &args) {
	djcall_args ra;

	djfs_request req;
	djfs_reply res;
	if (!str2xdr(req, args.data)) {
	    warn << "posixfs: cannot unmarshal input args\n";
	    cb_(REPLY_SYSERR, ra);
	    return;
	}

	res.set_err(0);
	res.d->set_op(req.op);

	try {
	    switch (req.op) {
	    case DJFS_READDIR: {
		DIR *d = opendir(req.readdir->pn.cstr());
		errcheck(!d);
		scope_guard<int, DIR*> cleanup(closedir, d);

		vec<str> ents;
		for (;;) {
		    struct dirent *de = readdir(d);
		    if (!de)
			break;

		    ents.push_back(str(de->d_name));
		}

		res.d->readdir->ents.setsize(ents.size());
		uint32_t i;
		for (i = 0; i < ents.size(); i++)
		    res.d->readdir->ents[i] = ents[i];
		break;
	    }

	    case DJFS_READ: {
		int fd = open(req.read->pn.cstr(), O_RDONLY);
		errcheck(fd < 0);
		scope_guard<int, int> cleanup(close, fd);

		struct stat st;
		errcheck(fstat(fd, &st));

		res.d->read->data.setsize(st.st_size);
		errcheck(read(fd, res.d->read->data.base(), st.st_size) != st.st_size);
		break;
	    }

	    default:
		warn << "posixfs: unknown op " << req.op << "\n";
		cb_(REPLY_SYSERR, ra);
		return;
	    }
	} catch (errno_exception &e) {
	    res.set_err(e.err());
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
