#include <lwip/sockets.h>

#define LWIP_PROTECTED_CALL(suffix, ...)	\
    ({						\
	int __r;				\
	lwip_core_lock();			\
	__r = lwip_ ## suffix(__VA_ARGS__);	\
	lwip_core_unlock();			\
	__r;					\
    })

#define socket(A, B, C) LWIP_PROTECTED_CALL(socket, A, B, C)
#define bind(A, B, C)   LWIP_PROTECTED_CALL(bind, A, B, C)
#define listen(A, B)    LWIP_PROTECTED_CALL(listen, A, B)
#define accept(A, B, C) LWIP_PROTECTED_CALL(accept, A, B, C)
#define read(A, B, C)   LWIP_PROTECTED_CALL(read, A, B, C)
#define write(A, B, C)  LWIP_PROTECTED_CALL(write, A, B, C)
#define close(A)        LWIP_PROTECTED_CALL(close, A)
