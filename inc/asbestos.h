#ifndef ASBESTOS_H
#define ASBESTOS_H
#include <machine/atomic.h>

// Labels

typedef struct label {
	size_t size;
	level_t default_level;
	handle_t *handles;
} label_t;

#endif /* ASBESTOS_H */
