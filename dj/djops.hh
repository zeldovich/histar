#ifndef JOS_DJ_DJOPS_HH
#define JOS_DJ_DJOPS_HH

extern "C" {
#include <inc/container.h>
#include <inc/string.h>
}

#include <sfscrypt.h>
#include <dj/djprotx.h>
#include <inc/cpplabel.hh>

struct dj_msg_id {
    dj_pubkey key;
    uint64_t xid;

    dj_msg_id(const dj_pubkey &k, uint64_t x) : key(k), xid(x) {}
    operator hash_t() const { return xid; }
};

inline dj_pubkey
sfspub2dj(ptr<sfspub> sfspub)
{
    dj_pubkey dpk;
    assert(sfspub->export_pubkey(&dpk));
    return dpk;
}

inline bool
operator<(const dj_pubkey &a, const dj_pubkey &b)
{
    if (a.type != SFS_RABIN || b.type != SFS_RABIN) {
	warn << "comparing non-rabin public keys (<)..\n";
	return true;
    }

    return *a.rabin < *b.rabin;
}

inline bool
operator==(const dj_pubkey &a, const dj_pubkey &b)
{
    if (a.type != SFS_RABIN || b.type != SFS_RABIN) {
	warn << "comparing non-rabin public keys (==)..\n";
	return false;
    }

    return *a.rabin == *b.rabin;
}

inline bool
operator!=(const dj_pubkey &a, const dj_pubkey &b)
{
    return !(a == b);
}

inline bool
operator<(const dj_msg_id &a, const dj_msg_id &b)
{
    return a.xid < b.xid || (a.xid == b.xid && a.key < b.key);
}

inline bool
operator==(const dj_msg_id &a, const dj_msg_id &b)
{
    return a.key == b.key && a.xid == b.xid;
}

inline bool
operator!=(const dj_msg_id &a, const dj_msg_id &b)
{
    return !(a == b);
}

inline bool
operator<(const dj_gcat &a, const dj_gcat &b)
{
    return (a.integrity < b.integrity) ||
	   (a.integrity == b.integrity &&
		((a.id < b.id) ||
		 (a.id == b.id && a.key < b.key)));
}

inline bool
operator==(const dj_gcat &a, const dj_gcat &b)
{
    return a.key == b.key && a.id == b.id && a.integrity == b.integrity;
}

inline bool
operator!=(const dj_gcat &a, const dj_gcat &b)
{
    return !(a == b);
}

inline bool
operator==(const cobj_ref &a, const cobj_ref &b)
{
    return a.container == b.container && a.object == b.object;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const dj_pubkey &pk)
{
    ptr<sfspub> p = sfscrypt.alloc(pk, 0);
    strbuf tsb;
    if (p && p->export_pubkey(tsb))
	sb << tsb;
    else
	sb << "--cannot-convert--";
    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const dj_gcat &gc)
{
    sb << "<" << (gc.integrity ? "I" : "S")	/* integrity/secrecy */
	      << "." << gc.key << "." << gc.id << ">";
    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const dj_address &a)
{
    in_addr ia;
    ia.s_addr = a.ip;
    sb << inet_ntoa(ia) << ":" << ntohs(a.port);
    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const dj_entity &dje)
{
    switch (dje.type) {
    case ENT_PUBKEY:
	sb << *dje.key;
	break;

    case ENT_GCAT:
	sb << *dje.gcat;
	break;

    case ENT_ADDRESS:
	sb << *dje.addr;
	break;

    default:
	sb << "unknown";
    }

    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const dj_label &l)
{
    sb << "{";
    for (uint64_t i = 0; i < l.ents.size(); i++)
	sb << " " << l.ents[i] << ",";
    sb << " }";
    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const cobj_ref &c)
{
    sb << c.container << "." << c.object;
    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const label &l)
{
    sb << l.to_string();
    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const dj_message_endpoint &ep)
{
    switch (ep.type) {
    case EP_GATE:
	sb << "G:" << ep.ep_gate->msg_ct << "+"
		   << ep.ep_gate->gate.gate_ct << "."
		   << ep.ep_gate->gate.gate_id;
	break;

    default:
	sb << "{unknown EP type " << ep.type << "}";
    }
    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const dj_message &a)
{
    sb << "target ep:    " << a.target << "\n";
    sb << "taint:        " << a.taint << "\n";
    sb << "grant label:  " << a.glabel << "\n";
    sb << "grant clear:  " << a.gclear << "\n";
    sb << "catmap, dset: not printed\n";
    sb << "payload:      " << str(a.msg.base(), a.msg.size()) << "\n";
    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const dj_delivery_code &c)
{
    return rpc_print(sb, c, 0, 0, 0);
}

template<> struct hashfn<cobj_ref> {
    hashfn() {}
    hash_t operator() (const cobj_ref &a) const
	{ return a.container ^ a.object; }
};

template<> struct hashfn<dj_gcat> {
    hashfn() {}
    hash_t operator() (const dj_gcat &a) const
	{ return a.id; }
};

template<> struct hashfn<dj_pubkey> {
    hashfn() {}
    hash_t operator() (const dj_pubkey &pk) const
	{ return pk.type == SFS_RABIN ? pk.rabin->getu64() : 0; }
};

inline void
operator<<=(cobj_ref &c, str s)
{
    char *dot = strchr(s.cstr(), '.');
    if (dot) {
	*dot = '\0';
	strtou64(dot + 1, 0, 10, &c.object);
    }
    strtou64(s.cstr(), 0, 10, &c.container);
}

inline void
operator<<=(dj_gatename &c, str s)
{
    char *dot = strchr(s.cstr(), '.');
    if (dot) {
	*dot = '\0';
	strtou64(dot + 1, 0, 10, &c.gate_id);
    }
    strtou64(s.cstr(), 0, 10, &c.gate_ct);
}

#endif
