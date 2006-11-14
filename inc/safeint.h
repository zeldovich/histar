#ifndef JOS_INC_SAFEINT_H
#define JOS_INC_SAFEINT_H

static __inline __attribute__((always_inline)) uint64_t
safe_add(int *of, uint64_t a, uint64_t b)
{
    uint64_t r = a + b;
    if (r < a || r < b)
	*of = 1;
    return r;
}

static __inline __attribute__((always_inline)) uint64_t
safe_mul(int *of, uint64_t a, uint64_t b)
{
    uint64_t r = a * b;
    if (r / a != b)
	*of = 1;
    return r;
}

#endif
