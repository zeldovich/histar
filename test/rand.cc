#include <test/josenv.hh>
#include <test/rand.hh>

#include <string.h>

extern "C" {
#include <inc/sha1.h>
#include <inc/arc4.h>
#include <inc/bf60.h>
}

static arc4 a4;
static bf_ctx bfc;

uint64_t
x_hash(uint64_t input1, uint64_t input2)
{
    sha1_ctx ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, (unsigned char *) &input1, 8);
    sha1_update(&ctx, (unsigned char *) &input2, 8);

    unsigned char digest[20];
    sha1_final(&digest[0], &ctx);

    uint64_t hv;
    memcpy(&hv, &digest[0], sizeof(hv));
    return hv;
}

uint64_t
x_rand(void)
{
    uint64_t rv = 0;
    for (int i = 0; i < 8; i++)
	rv = (rv << 8) | arc4_getbyte(&a4);
    return rv;
}

void
x_init(const char *s)
{
    arc4_setkey(&a4, s, strlen(s));
    bf_setkey(&bfc, s, strlen(s));
}

uint64_t
x_encrypt(uint64_t v)
{
    return bf64_encipher(&bfc, v);
}

uint64_t
x_decrypt(uint64_t v)
{
    return bf64_decipher(&bfc, v);
}
