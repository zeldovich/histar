#include <dj/dj.h>

inline bool
operator<(const dj_esign_pubkey &a, const dj_esign_pubkey &b)
{
    return a.n < b.n || (a.n == b.n && a.k < b.k);
}

inline const strbuf &
strbuf_cat(const strbuf &sb, const dj_esign_pubkey &pk)
{
    sb << "{" << pk.n << "," << pk.k << "}";
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
	sb << "<" << dje.gcat->key << "." << dje.gcat->id << ">";
	break;

    case ENT_ADDRESS:
	in_addr ia;
	ia.s_addr = dje.addr->ip;
	sb << inet_ntoa(ia) << ":" << ntohs(dje.addr->port);
    }

    return sb;
}

