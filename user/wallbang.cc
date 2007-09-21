extern "C" {
#include <inc/lib.h>
#include <inc/debug.h>
#include <inc/stdio.h>
#include <inc/syscall.h>

#include <string.h>
#include <stdio.h>
}

static const char send_dbg = 0;
static const char recv_dbg = 0;
static const char string_dbg = 1;

// for tx/rxing strings
static const char string_start = 0xAA;
static const char string_end = 0x00;

// AMD Athlon(tm) 64 Processor 3400+
static const uint64_t period = NSEC_PER_SECOND / 1000 * 50;
static const uint64_t iters = 10000;
static const uint64_t tolerance = NSEC_PER_SECOND / 1000 * 3;
static const uint64_t lo_thresh = period * 800000;

// moscow+qemu settings
//static const uint64_t period = NSEC_PER_SECOND / 1000 * 1000;
//static const uint64_t iters = 100000;
//static const uint64_t tolerance = NSEC_PER_SECOND / 1000 * 10;
//static const uint64_t lo_thresh = 2000000;

static void 
wallbang_align(void)
{
    uint64_t waiter = 0;
    uint64_t t = sys_clock_nsec();
    uint64_t d = t % period;
    if (d)
	sys_sync_wait(&waiter, 0, (period - d) + t);
}

static void
wallbang_sendbit(char bit)
{
    uint64_t waiter = 0;
    uint64_t now = sys_clock_nsec();
    uint64_t stop_target = (now - (now % period)) + period;
    debug_print(send_dbg, "sending %d (%ld)", bit ? 1 : 0, stop_target);
    debug_print(send_dbg, "now %ld, try to stop at %ld", now, stop_target);
    uint64_t stopped;
    
    while ((stopped = sys_clock_nsec()) < stop_target) {
	if (bit)
	    for (uint64_t i = 0; i < iters; i++) {}
	else if (stop_target - stopped > tolerance)
	    sys_sync_wait(&waiter, 0, stop_target - tolerance);
	else
	    sys_self_yield();
    }
    
    debug_print(send_dbg, "actually stopped at %ld", stopped);
}

static void __attribute__((unused))
wallbang_printbyte(char byte)
{
    for (uint64_t i = 0; i < sizeof(byte) * 8; i++)
	cprintf("%d", (byte & (1 << i)) ? 1 : 0);
}

static void
wallbang_sendbyte(char byte)
{
    for (uint64_t i = 0; i < sizeof(byte) * 8; i++)
	wallbang_sendbit(byte & (1 << i));
}

static void
wallbang_send(const char *buf, uint32_t n)
{
    debug_print(send_dbg, "attempting to send %d bytes", n);
    for (uint32_t i = 0; i < n; i++)
	wallbang_sendbyte(buf[i]);    
}

static void
wallbang_sendstr(const char *s)
{
    debug_print(send_dbg, "attempting to tx \"%s\" (%ld bytes)\n", 
		s, strlen(s));
    debug_print(string_dbg, "txing string start seq...");
    wallbang_align();
    wallbang_sendbit(1);
    wallbang_sendbyte(string_start);
    debug_print(string_dbg, "txing string contents...");
    wallbang_send(s, strlen(s));
    debug_print(string_dbg, "txing string end...");
    wallbang_sendbyte(string_end);
}


static void
wallbang_recvbit(char *bit)
{
    uint64_t now = sys_clock_nsec();
    uint64_t stop_target = (now - (now % period)) + period;
    debug_print(recv_dbg, "now %ld, try to stop at %ld", now, stop_target);
    uint64_t stopped;
    
    uint64_t count = 0;
    while ((stopped = sys_clock_nsec()) < stop_target) {
	for (uint64_t i = 0; i < iters; i++)
	    count++;
    }
    *bit = count < lo_thresh ? 1 : 0;
    debug_print(recv_dbg, "count %ld, bit %d (%ld)", count, *bit, stop_target);
    debug_print(recv_dbg, "actually stopped at %ld", stopped);
}

static void
wallbang_recvbyte(char *byte)
{
    *byte = 0;
    for (uint32_t i = 0; i < sizeof(*byte) * 8; i++) {
	char bit;
	wallbang_recvbit(&bit);
	*byte |= bit ? (1 << i)  : 0;
    }
}

static void __attribute__((unused))
wallbang_recv(void)
{
    cprintf("wallbang_recv: listening for wallbang_send...\n");
    
    wallbang_align();
    
    for (;;) {
	char c;
	wallbang_recvbyte(&c);
	debug_print(recv_dbg, "char %c", c);
    }

}

static void
wallbang_recvstr(char *buf, uint32_t n)
{
    char c = 0;

    debug_print(string_dbg, "waiting for string start seq...");

    wallbang_align();

    while (c != string_start) {
	while (!c)
	    wallbang_recvbit(&c);
	wallbang_recvbyte(&c);
    }
    
    debug_print(string_dbg, "rxed start seq!");
    
    uint32_t i = 0;
    for (; i < n - 1; i++) {
	wallbang_recvbyte(&c);
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
	cprintf("Usage: %s send | recv [data]\n", av[0]);
	return -1;
    }
        
    if (av[1][0] == 'r') {
	char buf[128];
	cprintf("wallbang: trying to recieve...\n");
	wallbang_recvstr(buf, sizeof(buf));
	cprintf("wallbang: recieved \"%s\"\n", buf);
    } else {
	const char *secret = 0;
	if (ac > 2)
	    secret = av[1];
	else
	    secret = "test 0123456789";
	cprintf("wallbang: trying to send \"%s\"\n", secret);
	wallbang_sendstr(secret);
    }

    return 0;
}
