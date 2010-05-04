#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <pkg/htcdream/smdd/msm_rpcrouter2.h>
#include <inc/smdd.h>
#include <pkg/htcdream/support/smddgate.h>
#include <pkg/htcdream/smdd/smd_rmnet.h>

int
main()
{
	struct rmnet_stats rs;

	smddgate_init();
	if (smddgate_rmnet_stats(0, &rs) == 0) {
		printf("RMNET STATISTICS:\n");
		printf("  %10u tx frames\n", rs.tx_frames);
		printf("  %10u tx bytes\n", rs.tx_frame_bytes);
		printf("  %10u tx frames dropped\n", rs.tx_dropped);
		printf("----------------------------------\n");
		printf("  %10u rx frames\n", rs.rx_frames);
		printf("  %10u rx bytes\n", rs.rx_frame_bytes);
		printf("  %10u rx frames dropped\n", rs.rx_dropped);
		printf("----------------------------------\n");
		printf("  %10u sec since last tx\n", (unsigned)(rs.tx_last_nsec / UINT64(1000000000)));
		printf("  %10u sec since last rx\n", (unsigned)(rs.rx_last_nsec / UINT64(1000000000)));
		return 0;
	} else {
		fprintf(stderr, "failed to get rmnet stats!\n");
		return -1;
	}
}
