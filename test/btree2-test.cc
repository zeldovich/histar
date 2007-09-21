extern "C" {
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <btree/btree.h>
#include <btree/error.h>
}

#include <test/rand.hh>
#include <inc/errno.hh>

#define RESERVED_PAGES 2000

enum { iterations = 10000 };
enum { num_keys = 500 };
enum { max_repeats = 100 };
enum { logging = 0 };

#define should_be(expr, val, string) \
    do {								\
	int64_t __e = (expr);						\
	int64_t __v = (val);						\
	if (__e != __v)							\
	    throw basic_exception("%s: %ld should be %ld",	\
		(string), __e, __v);		\
    } while (0)

// Some magic values for checking
static uint64_t magic1 = 0xdeadbeef00c0ffeeULL;
static uint64_t magic2 = 0xc0c0d0d0a0a0e0e0ULL;

struct btree_desc bd;
static char key_exists[num_keys];

static void
do_search(void)
{
    int rnd = (x_rand() % num_keys);
    uint64_t key = x_encrypt(rnd);
    uint64_t val[2];

    if (logging)
	printf("search key %lx\n", key);

    int r = btree_search(&bd, &key, &key, &val[0]);
    if (key_exists[rnd]) {
	should_be(r, 1, "search existing key");
	assert(key == x_encrypt(rnd));
	assert(val[0] == x_hash(key, magic1));
	assert(val[1] == x_hash(key, magic2));
    } else {
	should_be(r, 0, "search non-existent key");
    }
}

static void
do_traverse(void)
{
    if (logging)
	printf("traversal\n");

    biter_t bi;
    should_be(biter_init(&bd, &bi), 0, "init traversal");

    uint64_t key;
    uint64_t val[2];

    int count_tree = 0;
    while (biter_next(&bi, &key, val)) {
	int rnd = x_decrypt(key);
	if (rnd < 0 || rnd >= num_keys || !key_exists[rnd])
	    throw basic_exception("traversal: non-existant key %lx [%d] (%lx %lx)",
				  key, rnd, val[0], val[1]);
	assert(val[0] == x_hash(key, magic1));
	assert(val[1] == x_hash(key, magic2));
	count_tree++;
    }

    int count_should_be = 0;
    for (int i = 0; i < num_keys; i++)
	if (key_exists[i])
	    count_should_be++;

    assert(count_tree == count_should_be);
}

static void
do_delete(void)
{
    int rnd = (x_rand() % num_keys);
    uint64_t key = x_encrypt(rnd);

    if (logging)
	printf("delete key %lx\n", key);

    int r = btree_delete(&bd, &key);
    btree_verify(&bd);
    //btree_print(&bd);
    //printf("------\n");

    if (key_exists[rnd]) {
	should_be(r, 0, "delete existing key");
	key_exists[rnd] = 0;
    } else {
	should_be(r, -E_NOT_FOUND, "delete missing key");
    }
}

static void
do_insert(void)
{
    int rnd = (x_rand() % num_keys);
    uint64_t key = x_encrypt(rnd);
    uint64_t val[2] = { x_hash(key, magic1), x_hash(key, magic2) };

    if (logging)
	printf("insert key %lx val %lx %lx\n", key, val[0], val[1]);

    int r = btree_insert(&bd, &key, &val[0]);
    //btree_print(&bd);
    //printf("------\n");
    if (key_exists[rnd]) {
	should_be(r, -E_INVAL, "insert existing key");
    } else if (r < 0) {
	should_be(r, 0, "error?");
    } else {
	key_exists[rnd] = 1;
    }
}

static void
do_verify(void)
{
    if (logging)
	printf("verify check\n");

    btree_verify(&bd);
}

static struct {
    void (*fn) (void);
    int randomized;
    int weight;
} ops[] = {
    { &do_insert,	1,	100	},
    { &do_delete,	1,	100	},
    { &do_search,	1,	50	},
    { &do_traverse,	0,	50	},
    { &do_verify,	0,	100	},
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
	    int repeat_count = x_rand() % max_repeats;
	    if (!ops[i].randomized)
		repeat_count = 1;

	    for (int j = 0; j < repeat_count; j++) {
		alarm(5);
		ops[i].fn();
		alarm(0);
	    }

	    return;
	}
	rv -= ops[i].weight;
    }

    printf("do_something: bad weight logic?\n");
}

static void
timeout(int signo)
{
    printf("timed out\n");
    exit(-1);
}

int 
main(int ac, char **av)
{
    signal(SIGALRM, &timeout);

    x_init("Helloooo randomness!");
    
    assert_error(btree_alloc(50, 1, 2, &bd));

    //btree_desc_print(&bd);
    //return 0;
    
    for (int i = 0; i < iterations; i++)
	do_something();

    assert_error(btree_free(&bd));
    return 0;
}
