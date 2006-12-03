#include <lwip/api.h>
#include <api/socketdef.h>

void jos64_init_api(void);

void jos64_event_helper(int s, enum netconn_evt evt);
int jos64_sync_helper(int s, char write);
