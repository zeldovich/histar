#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct experiment {
	int pktsize, pktpersec, numpkts; 
} experiments[] = {
	{ 1500, 40, 400  },
	{ 1500, 20, 200  },
	{ 1500, 10, 100  },
	{ 1500, 5,   50  },
	{ 1500, 2,   20  },
	{ 1500, 1,   10  },

	{  750, 40,  400 },
	{  750, 20,  200 },
	{  750, 10,  100 },
	{  750,  5,   50 },
	{  750,  2,   20 },
	{  750,  1,   10 },

	{    1, 40,  400 },
	{    1, 20,  200 },
	{    1, 10,  100 },
	{    1,  5,   50 },
	{    1,  2,   20 },
	{    1,  1,   10 },

	{    0,  0,    0 }
};

#define SLEEP_BETWEEN_TESTS	30

int
main(int argc, char **argv)
{
	if (argc != 3 && argc != 4) {
		fprintf(stderr, "ERROR: need 2 or 3 args\n");
		exit(1);
	}

	char *dest_host = argv[1];
	char *dest_port = argv[2];

	for (struct experiment *exp = &experiments[0]; exp->pktsize != 0; exp++) {
		char cmd[1024];

		sprintf(cmd, "/bin/udptest %s %s %d %d %d",
		    dest_host, dest_port, exp->pktsize, exp->pktpersec, exp->numpkts); 

		if (argc == 4) {
			strcat(cmd, " ");
			strcat(cmd, argv[3]);
		}

		printf("running [%s]\n", cmd);
		system(cmd);

		sleep(SLEEP_BETWEEN_TESTS);
	}
}
