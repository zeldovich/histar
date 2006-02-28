#ifndef JOS_INC_SHA1_H
#define JOS_INC_SHA1_H

#include <inc/types.h>

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} sha1_ctx;

void sha1_init(sha1_ctx* context);
void sha1_update(sha1_ctx* context, unsigned char* data, uint32_t len);
void sha1_final(unsigned char digest[20], sha1_ctx* context);

#endif
