extern "C" {
#include <inc/string.h>
#include <inc/gateparam.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
}

#include <inc/gateclnt.hh>
#include <inc/error.hh>
#include <inc/rild.h>

#include "../support/misc.h"
#include "../rild/ril/ril.h"

static void
usage(const char *progname)
{
	fprintf(stderr, "usage: %s [command] [param1] [param2] ...\n");
	fprintf(stderr, "  commands:\n");
	fprintf(stderr, "    radioon           - turn radio on and join network\n");
	fprintf(stderr, "    radiooff          - turn radio off\n");
	fprintf(stderr, "    sendsms [smsstr]  - send sms message (smsstr must be hex)\n");
	fprintf(stderr, "    dial [number]     - dial phone number\n");
	fprintf(stderr, "    answer            - answer phone\n");
	fprintf(stderr, "    end               - end call (hang up)\n");
	fprintf(stderr, "    ifacever          - get RIL interface version\n");
	fprintf(stderr, "    libver            - get RIL .so library version string\n");
	fprintf(stderr, "    state             - get RIL state\n");
	exit(1);
}

static const char *state2str[5] = {
	"off",
	"not initialized",
	"radio on, sim not ready",
	"radio on, sim locked or absent",
	"radio on, sim ready"
};

int
main(int argc, char **argv)
try
{
	struct gate_call_data gcd;
	struct rild_req *req = (struct rild_req *)&gcd.param_buf[0];
	struct rild_reply *rep = (struct rild_reply *)&gcd.param_buf[0];

	if (argc < 2 ) {
		fprintf(stderr, "%s: no command given\n", argv[0]);
		usage(argv[0]);
	}

	int64_t rildgt = container_find(start_env->root_container, kobj_gate, "rild gate");
	error_check(rildgt);
	struct cobj_ref rildgate = COBJ(start_env->root_container, rildgt);

	if (strcmp(argv[1], "radioon") == 0) {
		req->op = radio_on;
		gate_call(rildgate, 0, 0, 0).call(&gcd, 0);
	} else if (strcmp(argv[1], "radiooff") == 0) {
		req->op = radio_off;
		gate_call(rildgate, 0, 0, 0).call(&gcd, 0);
	} else if (strcmp(argv[1], "sendsms") == 0) {
		if (argc != 3)
			usage(argv[0]);
		req->op = send_sms;
		strcpy(req->buf, argv[2]);
		req->bufbytes = strlen(argv[2]);
		gate_call(rildgate, 0, 0, 0).call(&gcd, 0);
		printf("sending sms [%s]\n", argv[2]);
	} else if (strcmp(argv[1], "dial") == 0) {
		if (argc != 3)
			usage(argv[0]);
		req->op = dial_number;
		strcpy(req->buf, argv[2]);
		req->bufbytes = strlen(argv[2]);
		gate_call(rildgate, 0, 0, 0).call(&gcd, 0);
		printf("dialing %s.\n", argv[2]);
	} else if (strcmp(argv[1], "answer") == 0) {
		req->op = answer_call;
		gate_call(rildgate, 0, 0, 0).call(&gcd, 0);
		printf("answering call.\n");
	} else if (strcmp(argv[1], "end") == 0) {
		req->op = end_call;
		gate_call(rildgate, 0, 0, 0).call(&gcd, 0);
		printf("ending call.\n");
	} else if (strcmp(argv[1], "ifacever") == 0) {
		req->op = get_ril_interface_version;
		gate_call(rildgate, 0, 0, 0).call(&gcd, 0);
		if (rep->err >= 0)
			printf("ril interface version: %d\n", rep->err);
		else
			fprintf(stderr, "failed to obtain ril interface version\n");
	} else if (strcmp(argv[1], "libver") == 0) {
		req->op = get_ril_lib_version;
		gate_call(rildgate, 0, 0, 0).call(&gcd, 0);
		if (rep->bufbytes > 0)
			printf("ril library version string: [%s]\n", rep->buf);
		else
			fprintf(stderr, "failed to obtain ril library version string\n");
	} else if (strcmp(argv[1], "state") == 0) {
		req->op = get_ril_state;
		gate_call(rildgate, 0, 0, 0).call(&gcd, 0);
		if (rep->err >= 0 && rep->err <= 4)
			printf("ril state: %d (%s)\n", rep->err, state2str[rep->err]);
		else
			fprintf(stderr, "failed to obtain ril state\n");
	} else {
		fprintf(stderr, "%s: invalid command %s\n", argv[0], argv[1]);
		usage(argv[0]);
	}

	return (0);
} catch (std::exception &e) {
	printf("rilc: %s\n", e.what());
}
