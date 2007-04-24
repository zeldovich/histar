#include <kern/handle.h>
#include <kern/lib.h>
#include <inc/bf60.h>

enum { handle_encrypt = 1 };

uint8_t system_key[SYSTEM_KEY_SIZE];

struct bf_ctx pstate_key_ctx;
static struct bf_ctx handle_key_ctx;
uint64_t handle_counter;

void
key_generate(void)
{
    // Probably should be something more interesting later on
    for (int i = 0; i < SYSTEM_KEY_SIZE; i++)
	system_key[i] = i + 5;

    key_derive();
}

static uint64_t
key_derive_one(struct bf_ctx *src, struct bf_ctx *dst, uint64_t ctr)
{
    uint8_t keybuf[SYSTEM_KEY_SIZE];
    memset(&keybuf[0], 0, sizeof(keybuf));

    for (uint64_t i = 0; i < SYSTEM_KEY_SIZE; i++) {
	uint64_t v = 0;
	for (uint8_t j = 0; j < 8; j++) {
	    v <<= 8;
	    v |= (i + ctr);
	}

	keybuf[i] = bf64_encipher(src, v);
    }

    bf_setkey(dst, &keybuf[0], SYSTEM_KEY_SIZE);
    return ctr + SYSTEM_KEY_SIZE;
}

void
key_derive(void)
{
    static struct bf_ctx init_ctx;
    bf_setkey(&init_ctx, &system_key[0], SYSTEM_KEY_SIZE);

    uint64_t ctr = 0;
    ctr = key_derive_one(&init_ctx, &handle_key_ctx, ctr);
    ctr = key_derive_one(&init_ctx, &pstate_key_ctx, ctr);
    assert(ctr < 0x100);
}

uint64_t
handle_alloc(void)
{
    uint64_t new_count = handle_counter++;
    uint64_t new_crypt = bf61_encipher(&handle_key_ctx, new_count);

    uint64_t h = handle_encrypt ? new_crypt : new_count;
    if (h == 0)
	h = handle_alloc();

    return h;
}
