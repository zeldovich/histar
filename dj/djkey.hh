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
	const dj_stmt_signed ss;
	const dj_delegation &d;
	dj_pubkey pk;

	dm_ent(const dj_stmt_signed &s) : ss(s), d(*ss.stmt.delegation), pk(*d.a.key) {}
    };

    ~dj_delegation_map() { t_.deleteall(); }
    dj_delegation_map() : size_(0) {}
    dj_delegation_map(const dj_delegation_set &dset) : size_(0) {
	insert(dset);
    }

    uint32_t size() { return size_; }
    void insert(const dj_delegation_set &dset);
    void insert(const dj_delegation_map &dmap);
    void insert(const dj_stmt_signed &ss);
    dj_delegation_set to_delegation_set();

    itree<dj_pubkey, dm_ent, &dm_ent::pk, &dm_ent::link> t_;

 private:
    uint32_t size_;
    dj_delegation_map(const dj_delegation_map&);
    dj_delegation_map &operator=(const dj_delegation_map&);
};

bool key_speaks_for(const dj_pubkey &k, const dj_gcat &gcat,
		    dj_delegation_map &dm, uint32_t depth);

#endif
