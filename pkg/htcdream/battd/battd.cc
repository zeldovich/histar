extern "C" {
#include <inc/string.h>
#include <inc/gateparam.h>
#include <inc/reserve.h>
#include <inc/syscall.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
}

#include "../smdd/msm_rpcrouter2.h"
#include <inc/smdd.h>

#include <inc/gatesrv.hh>

#include "../support/misc.h"
#include "../support/smddgate.h"

static uint64_t batt_max;	// battery's max capacity in J
struct cobj_ref rootrs;		// root capacitor cobj_ref
static uint64_t last_time;	// last % change handled when (in msec)
static uint64_t last_rs_level;	// last level in root reserve

static void
handle_change(int last_pct, int now_pct)
{
	int delta = last_pct - now_pct;
	struct ReserveInfo ri;

	if (sys_reserve_get_info(rootrs, &ri)) {
		fprintf(stderr, "%s: failed to read root reserve info\n");
		exit(1);
	}

	// XXX- need to set this properly w/ syscall once battery first read
	if (ri.rs_level > (batt_max * 1000)) {
		fprintf(stderr, "%s: root reserve level > battery max!\n");
		exit(1);
	}

	if (delta < 0) {
		fprintf(stderr, "battery delta: %d; increased charge\n", delta);
		return;
	}

	uint64_t mJ_per_pct = batt_max / 100;
	uint64_t mJ_used = mJ_per_pct * delta;
	uint64_t rs_mJ_used = last_rs_level - ri.rs_level;
	uint64_t this_time = sys_clock_nsec() / (1000 * 1000);

	printf("----------------------------------------\n");
	printf("batt now at %d%% (was %d%%)\n", now_pct, last_pct);
	printf("batt delta %d%% in %" PRIu64 " milliseconds\n", delta,
	     this_time - last_time);
	printf("%" PRIu64 " mJ per 1%% battery\n", mJ_per_pct);
	printf("%" PRIu64 " mJ used since last (%" PRIu64 "mW)\n", mJ_used,
	    mJ_used / ((this_time - last_time) / 1000));
	printf("%" PRIu64 " mJ consumed in reserve\n", rs_mJ_used); 
	printf("----------------------------------------\n");

	if (rs_mJ_used < mJ_used) {
		printf("reserve used less than reality! off by %" PRIu64
		    " mJ\n", mJ_used - rs_mJ_used);
	} else if (mJ_used < rs_mJ_used) {
		printf("reserve used more than battery! off by %" PRIu64
		    " mJ\n", rs_mJ_used - mJ_used);
	}

	last_time = this_time;
}

int
main(int argc, char **argv)
try
{
	struct htc_get_batt_info_rep batt_info;

	if (argc != 1 && argc != 2) {
		fprintf(stderr, "usage: %s [batt_max_J]\n", argv[0]);
		exit(1);
	}

	if (argc == 2) {
		strtou64(argv[1], NULL, 10, &batt_max);
		batt_max *= 1000;	//to mJ
	}

	int64_t rsid = container_find(start_env->root_container,
	    kobj_reserve, "root_reserve");
	if (rsid < 0)
		perror("couldn't find root_reserve");
	rootrs = COBJ(start_env->root_container, rsid); 

	if (smddgate_init()) {
		fprintf(stderr, "battd: smddgate_init failed\n");
		exit(1);
	}

	// initialise
	//     but wait for smdd to come online
	int last_pct;
	while (1) {
		if (smddgate_get_battery_info(&batt_info) == 0) {
			last_pct = be32_to_cpu(batt_info.info.level);
			last_time = sys_clock_nsec() / (1000 * 1000);
			break;
		}
	}
	struct ReserveInfo ri;
	if (sys_reserve_get_info(rootrs, &ri)) {
		fprintf(stderr, "%s: sys_reserve_get_info failed\n", argv[0]);
		exit(1);
	}
	last_rs_level = ri.rs_level;

	// 3.7v battery; htc tells us microamp-hours, so we get mJ from below
	uint64_t batt_claimed_max =
	    ((uint64_t)be32_to_cpu(batt_info.info.full_bat) * 37 * 3600) / 10000;

	if (batt_max == 0)
		batt_max = batt_claimed_max;
	else
		printf("%s: overriding batt_max with user input [%s]!\n", argv[0], argv[1]);

	printf("%s: battery claims %u mAh (%" PRIu64 "mJ)\n", argv[0],
	    be32_to_cpu(batt_info.info.full_bat)/1000, batt_claimed_max);
	printf("%s: battery initially at %d%% (%" PRIu64 "mJ)\n", argv[0],
	    last_pct, last_pct * (batt_max / 100));

	while (1) {
		if (smddgate_get_battery_info(&batt_info) == 0) {
			int pct = be32_to_cpu(batt_info.info.level);
			if (pct != last_pct)
				handle_change(last_pct, pct);
			last_pct = pct;
		}
		sleep(5);
	}

	return (0);
} catch (std::exception &e) {
	printf("battd: %s\n", e.what());
}
