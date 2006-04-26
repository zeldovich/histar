#ifndef JOS_INC_ARC4_H
#define JOS_INC_ARC4_H

struct arc4 {
    u_char i;
    u_char j;
    u_char s[256];
};
typedef struct arc4 arc4;

static inline u_char
arc4_getbyte(arc4 *a)
{
    u_char si, sj;
    a->i = (a->i + 1) & 0xff;
    si = a->s[a->i];
    a->j = (a->j + si) & 0xff;
    sj = a->s[a->j];
    a->s[a->i] = sj;
    a->s[a->j] = si;
    return a->s[(si + sj) & 0xff];
}

void arc4_reset(arc4 *a);
void arc4_setkey(arc4 *a, const void *_key, size_t len);

#endif
