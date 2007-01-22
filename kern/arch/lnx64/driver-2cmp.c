#include <machine/lnxinit.h>
#include <machine/actor.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/uinit.h>
#include <inc/arc4.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

extern void __k1_lnx64_init(const char *disk_pn, const char *cmdline, uint64_t membytes);
extern void __k2_lnx64_init(const char *disk_pn, const char *cmdline, uint64_t membytes);

extern void __k1_actor_create(struct actor *ar, int tainted);
extern void __k2_actor_create(struct actor *ar, int tainted);

extern void __k1_action_run(struct actor *ar, struct actor_context *ac, struct action *an, struct action_result *r);
extern void __k2_action_run(struct actor *ar, struct actor_context *ac, struct action *an, struct action_result *r);

static uint64_t
rnd(struct arc4 *a4)
{
    uint64_t rv = 0;
    for (int i = 0; i < 8; i++)
	rv = (rv << 8) | arc4_getbyte(a4);
    return rv;
}

static void
choose_action(struct arc4 *a4, struct action *an)
{
    memset(an, 0, sizeof(*an));
    an->type = rnd(a4) % actor_action_max;
}

int
main(int argc, char **av)
{
    const char *disk_pn = "/dev/null";
    const char *cmdline = "pstate=discard";

    // Technically this is pretty bad, because they're sharing the disk.
    // But, we aren't using the disk for anything yet, so whatever..
    __k1_lnx64_init(disk_pn, cmdline, 64 * 1024 * 1024);
    __k2_lnx64_init(disk_pn, cmdline, 64 * 1024 * 1024);

    printf("HiStar/lnx64: twice the number of kernels!\n");

    struct actor ar[2][2];
    __k1_actor_create(&ar[0][0], 0);
    __k1_actor_create(&ar[0][1], 1);

    __k2_actor_create(&ar[1][0], 0);
    __k2_actor_create(&ar[1][1], 1);

    struct arc4 a4;
    const char *seed = "hello world.";
    arc4_setkey(&a4, seed, strlen(seed));

    struct actor_context ac[2][2];
    memset(ac, 0, sizeof(ac));
    ac[0][1].handle_counter = (((uint64_t) 1) << 32);

    for (uint64_t round = 0; ; round++) {
	struct action_result r[2][2];
	struct action an0, an1;
	choose_action(&a4, &an0);
	choose_action(&a4, &an1);

	// Kernel #1 runs untainted actor and tainted actor
	__k1_action_run(&ar[0][0], &ac[0][0], &an0, &r[0][0]);
	__k1_action_run(&ar[0][1], &ac[0][1], &an1, &r[0][1]);

	// Kernel #2 runs only the untainted actor
	__k2_action_run(&ar[1][0], &ac[1][0], &an0, &r[1][0]);

	// Ideally, kernel #1 prevents tainted actor from influencing
	// the untainted actor, so the untainted actors should behave
	// exactly the same on both kernels.
	int mismatch = memcmp(&r[0][0], &r[1][0], sizeof(r[0][0]));
	printf("round=%"PRIu64", action=%d, mismatch=%d\n", round, an0.type, mismatch);

	if (mismatch) {
	    printf("Kernel 1 (mixed): rval=%"PRIu64"\n", r[0][0].rval);
	    printf("Kernel 2 (clean): rval=%"PRIu64"\n", r[1][0].rval);
	}
    }
}
