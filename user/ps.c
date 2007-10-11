#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/syscall.h>
#include <inc/stdio.h>

int
main(int ac, char **av)
{
    uint64_t poolct;
    char *ep;
    struct fs_inode ino;

    switch (ac) {
    case 1:
	// Assume current process pool
	poolct = start_env->process_pool;
	break;

    case 2:
	if (fs_namei(av[1], &ino) >= 0) {
	    poolct = ino.obj.object;
	    break;
	}

	if (strtou64(av[1], &ep, 0, &poolct) >= 0 && !*ep)
	    break;

    default:
	printf("Usage: %s [procpool-path|procpool-id]\n", av[0]);
	exit(1);
    }

    int64_t nslots = sys_container_get_nslots(poolct);
    if (nslots < 0) {
	printf("cannot read procpool %"PRIu64": %s\n", poolct, e2s(nslots));
	exit(1);
    }

    printf("%22s  %s\n", "PID", "CMD");

    for (int64_t i = 0; i < nslots; i++) {
	int64_t ctid = sys_container_get_slot_id(poolct, i);
	if (ctid < 0)
	    continue;

	int64_t psid = container_find(ctid, kobj_segment, "process status");
	if (psid < 0)
	    continue;

	struct process_state *procstat = 0;
	int r = segment_map(COBJ(ctid, psid), 0, SEGMAP_READ,
			    (void **) &procstat, 0, 0);
	if (r < 0) {
	    printf("Cannot map process status segment %"PRIu64".%"PRIu64": %s\n",
		   ctid, psid, e2s(r));
	    continue;
	}

	char buf[sizeof(procstat->procname) + 1];
	strncpy(&buf[0], &procstat->procname[0], sizeof(procstat->procname));
	segment_unmap_delayed(procstat, 1);

	buf[sizeof(buf) - 1] = '\0';
	printf("%22"PRIu64"  %s\n", ctid, buf);
    }
}
