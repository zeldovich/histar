extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/bipipe.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
}

#include <inc/errno.hh>

static const uint32_t num_threads = 5;
static const uint32_t num_fd = 5;

static void __attribute__((noreturn))
bipipe_worker(void *a)
{
    int fds[2][num_fd];
    for (;;) {
	for (uint32_t i = 0; i < num_fd; i++) {
	    cobj_ref seg;
	    error_check(bipipe_alloc(start_env->shared_container, &seg, 
				     0, "test-bipipe"));
	    errno_check(fds[0][i] = 
			bipipe_fd(seg, 0, 0, 0, 0));
	    errno_check(fds[1][i] = 
			bipipe_fd(seg, 1, 0, 0, 0));
	}
	
	for (uint32_t i = 0; i < num_fd; i++) {
	    errno_check(close(fds[0][i]));
	    errno_check(close(fds[1][i]));
	}
    }
}

static void __attribute__((noreturn))
worker(void *a)
{
    int fds[num_fd];
    for (;;) {
	for (uint32_t i = 0; i < num_fd; i++)
	    errno_check(fds[i] = open("/dev/null/", O_RDWR));
	
	for (uint32_t i = 0; i < num_fd; i++)
	    errno_check(close(fds[i]));
    }
}

int
main(int ac, char **av)
{
    cprintf("starting %d threads...\n", num_threads);
    
    cobj_ref t;
    for (uint32_t i = 0; i < num_threads; i++)
	thread_create(start_env->shared_container, worker, 0, &t, "worker");
	//thread_create(start_env->shared_container, bipipe_worker, 0, &t, "worker");
    
    thread_halt();
    return 0;
}
