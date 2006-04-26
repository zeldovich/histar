#include <inc/types.h>
#include <inc/arc4.h>

void
arc4_reset(arc4 *a)
{
    int n;
    a->i = 0xff;
    a->j = 0;
    for (n = 0; n < 0x100; n++)
	a->s[n] = n;
}

static void
_arc4_setkey(arc4 *a, const u_char *key, size_t keylen)
{
    u_int n, keypos;
    u_char si;
    for (n = 0, keypos = 0; n < 256; n++, keypos++) {
	if (keypos >= keylen)
	    keypos = 0;
	a->i = (a->i + 1) & 0xff;
	si = a->s[a->i];
	a->j = (a->j + si + key[keypos]) & 0xff;
	a->s[a->i] = a->s[a->j];
	a->s[a->j] = si;
    }
}

void
arc4_setkey(arc4 *a, const void *_key, size_t len)
{
    const u_char *key = (const u_char *) _key;
    arc4_reset(a);
    while (len > 128) {
	len -= 128;
	key += 128;
	_arc4_setkey(a, key, 128);
    }
    if (len > 0)
	_arc4_setkey(a, key, len);
    a->j = a->i;
}
