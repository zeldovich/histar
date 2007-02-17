#ifndef JOS_DJ_DJOPS_HH
#define JOS_DJ_DJOPS_HH

extern "C" {
#include <inc/string.h>
}

#include <crypt.h>
#include <esign.h>
#include <dj/dj.h>

struct dj_msg_id {
    dj_esign_pubkey key;
    uint64_t xid;

    dj_msg_id(const dj_esign_pubkey &k, uint64_t x) : key(k), xid(x) {}
    operator hash_t() const { return xid; }
};

inline bool
operator<(const dj_esign_pubkey &a, const dj_esign_pubkey &b)
{
    return a.n < b.n || (a.n == b.n && a.k < b.k);
}

inline bool
operator==(const dj_esign_pubkey &a, const dj_esign_pubkey &b)
{
    return a.n == b.n && a.k == b.k;
}

inline bool
operator!=(const dj_esign_pubkey &a, const dj_esign_pubkey &b)
{
    return !(a == b);
}

inline bool
operator<(const dj_msg_id &a, const dj_msg_id &b)
{
    return a.key < b.key || (a.key == b.key && a.xid < b.xid);
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
    return a.key < b.key || (a.key == b.key && a.id < b.id);
}

inline bool
operator==(const dj_gcat &a, const dj_gcat &b)
{
    return a.key == b.key && a.id == b.id;
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
strbuf_cat(const strbuf &sb, const dj_esign_pubkey &pk)
{
    sb << "{" << pk.n << "," << pk.k << "}";
    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const dj_gcat &gc)
{
    sb << "<" << gc.key << "." << gc.id << ">";
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
    }

    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const dj_label_entry &e)
{
    sb << e.cat << ":";
    if (e.level == LB_LEVEL_STAR)
	sb << "*";
    else
	sb << e.level;
    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const dj_label &l)
{
    sb << "{";
    for (uint64_t i = 0; i < l.ents.size(); i++)
	sb << " " << l.ents[i] << ",";
    if (l.deflevel == LB_LEVEL_STAR)
	sb << " *";
    else
	sb << " " << l.deflevel;
    sb << " }";
    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const dj_catlist &g)
{
    sb << "{";
    for (uint64_t i = 0; i < g.cats.size(); i++)
	sb << " " << g.cats[i];
    sb << " }";
    return sb;
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const cobj_ref &c)
{
    sb << c.container << "." << c.object << "\n";
    return sb;
}

inline dj_esign_pubkey
esignpub2dj(const esign_pub &ep)
{
    dj_esign_pubkey pk;
    pk.n = ep.n;
    pk.k = ep.k;
    return pk;
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

template<> struct hashfn<dj_esign_pubkey> {
    hashfn() {}
    hash_t operator() (const dj_esign_pubkey &a) const
	{ str r = a.n.getraw(); return hash_bytes(r.cstr(), r.len()) ^ a.k; }
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
