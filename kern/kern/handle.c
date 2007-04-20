#include <kern/handle.h>
#include <inc/bf60.h>

static int handle_encrypt = 1;

static struct bf_ctx handle_key_ctx;
static int handle_key_ctx_inited;

uint8_t handle_key[HANDLE_KEY_SIZE];
uint64_t handle_counter;

static void
handle_key_ctx_init(void)
{
    if (handle_key_ctx_inited == 0) {
	handle_key_ctx_inited = 1;
	bf_setkey(&handle_key_ctx, &handle_key[0], HANDLE_KEY_SIZE);
    }
}

void
handle_key_generate(void)
{
    // Probably should be something more interesting later on
    for (int i = 0; i < HANDLE_KEY_SIZE; i++)
	handle_key[i] = i + 5;
}

uint64_t
handle_alloc(void)
{
    handle_key_ctx_init();

    uint64_t new_count = handle_counter++;
    uint64_t new_crypt = bf61_encipher(&handle_key_ctx, new_count);

    uint64_t h = handle_encrypt ? new_crypt : new_count;
    if (h == 0)
	h = handle_alloc();

    return h;
}

uint64_t
handle_decrypt(uint64_t h)
{
    uint64_t d = bf61_decipher(&handle_key_ctx, h);
    return handle_encrypt ? d : h;
}
