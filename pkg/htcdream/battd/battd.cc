extern "C" {
#include <inc/string.h>
#include <inc/gateparam.h>

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

int
main(int argc, char **argv)
try
{
	struct htc_get_batt_info_rep batt_info;

	if (smddgate_init()) {
		fprintf(stderr, "battd: smddgate_init failed\n");
		exit(1);
	}

	while (1) {
		if (smddgate_get_battery_info(&batt_info) == 0) {
			fprintf(stderr, "battery current: %d\n", be32_to_cpu(batt_info.info.batt_current));
		}
		sleep(1);
	}

	return (0);
} catch (std::exception &e) {
	printf("battd: %s\n", e.what());
}
