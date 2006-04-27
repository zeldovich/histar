#include <test/josenv.hh>
#include <test/disk.hh>
#include <test/rand.hh>

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

extern "C" {
#include <inc/sha1.h>
#include <inc/error.h>
#include <btree/btree.h>
#include <kern/freelist.h>
#include <kern/log.h>
#include <kern/disklayout.h>
}

#include <inc/error.hh>

enum { iterations = 10000 };
enum { num_keys = 1000 };
enum { logging = 1 };

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
    int rnd = (x_rand() % num_keys);
    uint64_t key = x_hash(rnd, magic1);
    uint64_t val[2] = { x_hash(key, magic1), x_hash(key, magic2) };

    if (logging)
	printf("insert key %lx val %lx %lx\n", key, val[0], val[1]);

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
    int rnd = (x_rand() % num_keys);
    uint64_t key = x_hash(rnd, magic1);
    uint64_t val[2];

    if (logging)
	printf("search key %lx\n", key);

    int r = btree_search(BTREE_OBJMAP, &key, &key, &val[0]);
    if (key_exists[rnd]) {
	should_be(r, 0, "search existing key");
	assert(key == x_hash(rnd, magic1));
	assert(val[0] == x_hash(key, magic1));
	assert(val[1] == x_hash(key, magic2));
    } else {
	should_be(r, -E_NOT_FOUND, "search non-existent key");
    }
}

static void
do_search_leq(void)
{
    int rnd = (x_rand() % num_keys);
    uint64_t key = x_hash(rnd, magic1) + 1;
    uint64_t val[2];

    if (logging)
	printf("search leq %lx\n", key);

    int r = btree_ltet(BTREE_OBJMAP, &key, &key, &val[0]);
    if (r == 0 && key == x_hash(rnd, magic1) + 1) {
	printf("do_search_leq: coincidence?\n");
	return;
    }

    if (key_exists[rnd]) {
	should_be(r, 0, "search leq existing key");
	assert(key == x_hash(rnd, magic1));
	assert(val[0] == x_hash(key, magic1));
	assert(val[1] == x_hash(key, magic2));
    } else {
	if (r == 0)
	    assert(key != x_hash(rnd, magic1));
    }
}

static void
do_search_leq_e(void)
{
    int rnd = (x_rand() % num_keys);
    uint64_t key = x_hash(rnd, magic1);
    uint64_t val[2];

    if (logging)
	printf("search leq-e %lx\n", key);

    int r = btree_ltet(BTREE_OBJMAP, &key, &key, &val[0]);
    if (key_exists[rnd]) {
	should_be(r, 0, "search leq-e existing key");
	assert(key == x_hash(rnd, magic1));
	assert(val[0] == x_hash(key, magic1));
	assert(val[1] == x_hash(key, magic2));
    } else {
	if (r == 0)
	    assert(key != x_hash(rnd, magic1));
    }
}

static void
do_search_geq(void)
{
    int rnd = (x_rand() % num_keys);
    uint64_t key = x_hash(rnd, magic1) - 1;
    uint64_t val[2];

    if (logging)
	printf("search geq %lx\n", key);

    int r = btree_gtet(BTREE_OBJMAP, &key, &key, &val[0]);
    if (r == 0 && key == x_hash(rnd, magic1) - 1) {
	printf("do_search_geq: coincidence?\n");
	return;
    }

    if (key_exists[rnd]) {
	should_be(r, 0, "search geq existing key");
	assert(key == x_hash(rnd, magic1));
	assert(val[0] == x_hash(key, magic1));
	assert(val[1] == x_hash(key, magic2));
    } else {
	if (r == 0)
	    assert(key != x_hash(rnd, magic1));
    }
}

static void
do_search_geq_e(void)
{
    int rnd = (x_rand() % num_keys);
    uint64_t key = x_hash(rnd, magic1);
    uint64_t val[2];

    if (logging)
	printf("search geq-e %lx\n", key);

    int r = btree_gtet(BTREE_OBJMAP, &key, &key, &val[0]);
    if (key_exists[rnd]) {
	should_be(r, 0, "search geq-e existing key");
	assert(key == x_hash(rnd, magic1));
	assert(val[0] == x_hash(key, magic1));
	assert(val[1] == x_hash(key, magic2));
    } else {
	if (r == 0)
	    assert(key != x_hash(rnd, magic1));
    }
}

static void
do_delete(void)
{
    int rnd = (x_rand() % num_keys);
    uint64_t key = x_hash(rnd, magic1);

    if (logging)
	printf("delete key %lx\n", key);

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

static void
do_sanity_check(void)
{
    btree_sanity_check(BTREE_OBJMAP);
}

static struct {
    void (*fn) (void);
    int weight;
} ops[] = {
    { &do_insert,	10	},
    { &do_search,	500	},
    { &do_search_leq,	100	},
    { &do_search_geq,	100	},
    { &do_delete,	20	},
    { &do_flush,	0	},
    { &do_apply,	0	},
    { &do_sanity_check,	100	},
};

static void
do_something(void)
{
    int total_weight = 0;
    for (uint32_t i = 0; i < sizeof(ops) / sizeof(*ops); i++)
	total_weight += ops[i].weight;

    int rv = x_rand() % total_weight;
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

    x_srand("Helloooo randomness!");

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

    for (uint32_t i = 0; i < iterations; i++) {
	printf("i %d\n", i);
	do_something();
    }

    printf("All done.\n");
} catch (std::exception &e) {
    printf("exception: %s\n", e.what());

    btree_sanity_check(BTREE_OBJMAP);
    btree_pretty_print(BTREE_OBJMAP);
}
