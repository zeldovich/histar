#ifndef JOS_DJ_DJKEY_HH
#define JOS_DJ_DJKEY_HH

#include <itree.h>
#include <dj/djprotx.h>
#include <dj/djops.hh>

bool verify_stmt(const dj_stmt_signed &s);

template<class T>
bool
verify_sign(const T &xdrblob, const dj_pubkey &pk, const dj_sign &sig)
{
    str msg = xdr2str(xdrblob);
    if (!msg)
	return false;

    ptr<sfspub> p = sfscrypt.alloc(pk, SFS_VERIFY);
    return p && p->verify(sig, msg);
}

class dj_delegation_map {
 public:
    struct dm_ent {
	itree_entry<dm_ent> link;
	dj_pubkey pk;
	dj_delegation d;

	dm_ent(const dj_delegation &de) : pk(*de.a.key), d(de) {}
    };

    dj_delegation_map(const dj_delegation_set &dset) : size_(0) {
	for (uint32_t i = 0; i < dset.ents.size(); i++) {
	    dj_stmt_signed ss;
	    if (!bytes2xdr(ss, dset.ents[i]))
		continue;
	    if (!verify_stmt(ss))
		continue;
	    if (ss.stmt.type != STMT_DELEGATION)
		continue;
	    if (ss.stmt.delegation->a.type != ENT_PUBKEY)
		continue;
	    dm_ent *e = New dm_ent(*ss.stmt.delegation);
	    t_.insert(e);
	    size_++;
	}
    }

    ~dj_delegation_map() {
	t_.deleteall();
    }

    uint32_t size() { return size_; }

    itree<dj_pubkey, dm_ent, &dm_ent::pk, &dm_ent::link> t_;

 private:
    uint32_t size_;
    dj_delegation_map(const dj_delegation_map&);
    dj_delegation_map &operator=(const dj_delegation_map&);
};

bool key_speaks_for(const dj_pubkey &k, const dj_gcat &gcat,
		    dj_delegation_map &dm, uint32_t depth);

#endif
