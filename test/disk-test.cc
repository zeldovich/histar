#include <test/josenv.hh>
#include <test/disk.hh>

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

extern "C" {
#include <inc/error.h>
#include <btree/btree.h>
#include <kern/freelist.h>
#include <kern/log.h>
#include <kern/disklayout.h>
}

#include <inc/error.hh>

enum { iterations = 10000 };
enum { num_keys = 1000 };

#define errno_check(expr) \
    do {								\
	if ((expr) < 0)							\
	    throw basic_exception("%s: %s", #expr, strerror(errno));	\
    } while (0)

#define should_be(expr, val, string) \
    do {								\
	int64_t __e = (expr);						\
	int64_t __v = (val);						\
	if (__e != __v)							\
	    throw basic_exception("%s: %d (%s) should be %d (%s)",	\
		(string), __e, e2s(__e), __v, e2s(__v));		\
    } while (0)

// Global freelist..
struct freelist freelist;

// Some magic values for checking
static uint64_t magic1 = 0xdeadbeef00c0ffeeUL;
static uint64_t magic2 = 0xc0c0d0d0a0a0e0e0UL;

static char key_exists[num_keys];
static uint64_t log_size;

static void
do_insert(void)
{
    int rnd = (random() % num_keys);
    uint64_t key = rnd ^ magic1;
    uint64_t val[2] = { key * 3, magic2 + rnd };

    int r = btree_insert(BTREE_OBJMAP, &key, &val[0]);
    if (key_exists[rnd]) {
	should_be(r, -E_INVAL, "insert existing key");
    } else {
	key_exists[rnd] = 1;
    }
}

static void
do_search(void)
{
    int rnd = (random() % num_keys);
    uint64_t key = rnd ^ magic1;
    uint64_t val[2];

    int r = btree_search(BTREE_OBJMAP, &key, &key, &val[0]);
    if (key_exists[rnd]) {
	should_be(r, 0, "search existing key");
	assert(key == rnd ^ magic1);
	assert(val[0] == key * 3);
	assert(val[1] == magic2 + rnd);
    } else {
	should_be(r, -E_NOT_FOUND, "search non-existent key");
    }
}

static void
do_delete(void)
{
    int rnd = (random() % num_keys);
    uint64_t key = rnd ^ magic1;
    int r = btree_delete(BTREE_OBJMAP, &key);
    if (key_exists[rnd]) {
	should_be(r, 0, "delete existing key");
	key_exists[rnd] = 0;
    } else {
	should_be(r, -E_NOT_FOUND, "delete missing key");
    }
}

static void
do_flush(void)
{
    int flushed = log_flush();
    printf("flushed log: %d pages\n", flushed);
    log_size += flushed;
}

static void
do_apply(void)
{
    assert(0 == log_apply_disk(log_size));
    log_size = 0;
}

static struct {
    void (*fn) (void);
    int weight;
} ops[] = {
    { &do_insert, 100 },
    { &do_search, 500 },
    { &do_delete, 100 },
    { &do_flush,  50  },
    { &do_apply,  10  },
};

static void
do_something(void)
{
    int total_weight = 0;
    for (uint32_t i = 0; i < sizeof(ops) / sizeof(*ops); i++)
	total_weight += ops[i].weight;

    int rv = random() % total_weight;
    for (uint32_t i = 0; i < sizeof(ops) / sizeof(*ops); i++) {
	if (rv < ops[i].weight) {
	    ops[i].fn();
	    return;
	}
	rv -= ops[i].weight;
    }

    printf("do_something: bad weight logic?\n");
}

int
main(int ac, char **av)
try
{
    if (ac != 2) {
	printf("Usage: %s disk-file\n", av[0]);
	exit(-1);
    }

    srandom(0xdeadbef0);

    int fd;
    errno_check(fd = open(av[1], O_RDWR));

    struct stat st;
    error_check(fstat(fd, &st));

    int n_sectors = st.st_size / 512;
    if (n_sectors * 512 < RESERVED_PAGES * 4096) {
	printf("Disk too small, need at least %d bytes\n", RESERVED_PAGES*4096);
	exit(-1);
    }

    disk_init(fd, n_sectors * 512);

    // Try to do something..
    log_init();
    btree_manager_init();
    freelist_init(&freelist, RESERVED_PAGES * 4096, n_sectors * 512 - RESERVED_PAGES * 4096);

    for (uint32_t i = 0; i < iterations; i++)
	do_something();

    printf("All done.\n");
} catch (std::exception &e) {
    printf("exception: %s\n", e.what());
}
