extern "C" {
#include <inc/lib.h>
#include <inc/debug.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <machine/mmu.h>

#include <string.h>
#include <stdio.h>
}

static const char send_dbg = 0;
static const char recv_dbg = 1;
static const char align_dbg = 0;
static const char string_dbg = 1;

// for tx/rxing strings
static const char string_start = 0xAA;
static const char string_end = 0x00;

// for hs* machines
static const uint64_t period = 10000000;
static const uint64_t lo_thresh = 1267000;
static const uint64_t tolerance = 1000000;
// in bytes, must be a multiple of 8
static const uint32_t cache_size = 1024 * PGSIZE;
static volatile uint64_t *cache_array;

static void 
cachebang_align(void)
{
    uint64_t waiter = 0;
    uint64_t now = sys_clock_nsec();
    uint64_t stop_target = (now - (now % period)) + period;
    uint64_t stopped;

    debug_print(align_dbg, "now %ld, try to stop at %ld", now, stop_target);
    while ((stopped = sys_clock_nsec()) < stop_target) {
	if (stop_target - stopped > tolerance)
	    sys_sync_wait(&waiter, 0, stop_target - tolerance);
	else
	    sys_self_yield();
    }
    debug_print(align_dbg, "actually stopped at %ld", stopped);
}

static void
cachebang_sendbit(char bit)
{
    if (bit)
	for (uint32_t i = 0; i < (cache_size / 8); i++)
	    cache_array[i] = 0;
    
    cachebang_align();
}

static void __attribute__((unused))
cachebang_printbyte(char byte)
{
    for (uint64_t i = 0; i < sizeof(byte) * 8; i++)
	printf("%d", (byte & (1 << i)) ? 1 : 0);
}

static void
cachebang_sendbyte(char byte)
{
    
    for (uint64_t i = 0; i < sizeof(byte) * 8; i++)
	cachebang_sendbit(byte & (1 << i));
}

static void
cachebang_send(const char *buf, uint32_t n)
{
    debug_print(send_dbg, "attempting to send %d bytes", n);
    for (uint32_t i = 0; i < n; i++)
	cachebang_sendbyte(buf[i]);    
}

static void __attribute__((unused))
cachebang_sendstr(const char *s)
{
    debug_print(send_dbg, "attempting to tx \"%s\" (%ld bytes)\n", 
		s, strlen(s));
    debug_print(string_dbg, "txing string start seq...");
    cachebang_align();
    cachebang_sendbit(1);
    cachebang_sendbyte(string_start);
    debug_print(string_dbg, "txing string contents...");
    cachebang_send(s, strlen(s));
    debug_print(string_dbg, "txing string end...");
    cachebang_sendbyte(string_end);
}


static void
cachebang_recvbit(char *bit)
{
    uint64_t start = sys_clock_nsec();
    for (uint32_t i = 0; i < (cache_size / 8); i++)
	cache_array[i]++;

    uint64_t stop = sys_clock_nsec();
    *bit = ((stop - start) > lo_thresh) ? 1 : 0;
    debug_print(recv_dbg, "stop - start %ld lo_thresh %ld", 
		stop - start, lo_thresh);
    cachebang_align();
}

static void
cachebang_recvbyte(char *byte)
{
    *byte = 0;
    for (uint32_t i = 0; i < sizeof(*byte) * 8; i++) {
	char bit;
	cachebang_recvbit(&bit);
	*byte |= bit ? (1 << i)  : 0;
    }
}

static void __attribute__((unused))
cachebang_recv(void)
{
    printf("cachebang_recv: listening for cachebang_send...\n");
    
    cachebang_align();
    
    for (;;) {
	char c;
	cachebang_recvbyte(&c);
	debug_print(recv_dbg, "char %c", c);
    }

}

static void __attribute__((unused))
cachebang_recvstr(char *buf, uint32_t n)
{
    char c = 0;

    debug_print(string_dbg, "waiting for string start seq...");

    cachebang_align();

    while (c != string_start) {
	while (!c)
	    cachebang_recvbit(&c);
	cachebang_recvbyte(&c);
    }
    
    debug_print(string_dbg, "rxed start seq!");
    
    uint32_t i = 0;
    for (; i < n - 1; i++) {
	cachebang_recvbyte(&c);
	if (c == string_end)
	    break;
	buf[i] = c;
    }
    buf[i] = 0;
    debug_print(string_dbg, "rxed string \"%s\"", buf);
}

int
main(int ac, char **av)
{
    if (ac < 2) {
	printf("Usage: %s send | recv [data]\n", av[0]);
	return -1;
    }

    struct cobj_ref co;
    cache_array = 0;
    int r = segment_alloc(start_env->shared_container, cache_size, &co,
			  (void **)&cache_array, 0, "cache array");
    if (r < 0) {
	printf("cachebang: segment_alloc error: %s", e2s(r));
	return -1;
    }

#if 1
    if (av[1][0] == 'r') {
	printf("cachebang: trying to recieve...\n");
	cachebang_align();
	for (;;) {
	    char c;
	    cachebang_recvbit(&c);
	}
    } else {
	for (;;) {
	    printf("client: sending 10 1's\n");
	    for(int i = 0; i < 10; i++) 
		cachebang_sendbit(1);
	    printf("client: sending 10 0's\n");
	    for(int i = 0; i < 10; i++) 
		cachebang_sendbit(0);
	}
    }

#else
    
    if (av[1][0] == 'r') {
	char buf[128];
	printf("cachebang: trying to recieve...\n");
	cachebang_recvstr(buf, sizeof(buf));
	printf("cachebang: recieved \"%s\"\n", buf);
    } else {
	const char *secret = 0;
	if (ac > 2)
	    secret = av[1];
	else
	    secret = "test 0123456789";
	printf("cachebang: trying to send \"%s\"\n", secret);
	cachebang_sendstr(secret);
    }
#endif

    return 0;
}
