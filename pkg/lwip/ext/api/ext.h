#include <lwip/api.h>
#include <api/socketdef.h>

#include <lwip/opt.h>

void lwipext_init(char public_sockets);

#ifdef LWIPEXT_SYNC_ON
int  lwipext_sync_waiting(int s, char w);
void lwipext_sync_notify(int s, enum netconn_evt evt);
#else
#define lwipext_sync_waiting(s, w) 0
#define lwipext_sync_notify(s, e) 0
#endif
