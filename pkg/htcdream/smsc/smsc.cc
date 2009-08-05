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
#include <inc/smsd.h>

int
main(int argc, char **argv)
try
{
	struct gate_call_data gcd;
	struct smsd_req *req = (struct smsd_req *)&gcd.param_buf[0];
	struct smsd_reply *rep = (struct smsd_reply *)&gcd.param_buf[0];
	char *nl;

	if (argc != 1) {
		fprintf(stderr, "%s: this utility takes no argumemts\n", argv[0]);
		exit(1);
	}

	int64_t smsdgt = container_find(start_env->root_container, kobj_gate, "smsd gate");
	if (smsdgt < 0) {
		fprintf(stderr, "%s: smsd gate not found - cannot send sms messages\n",
		    argv[0]);
		exit(1);
	}
	struct cobj_ref smsdgate = COBJ(start_env->root_container, smsdgt);

	printf("SMS Destination Number: ");
	fflush(stdout);
	fgets(req->buf, sizeof(req->buf), stdin);
	nl = strchr(req->buf, '\n');
	if (nl != NULL)
		*nl = '\0';

	printf("SMS Message: ");
	fflush(stdout);
	fgets(req->buf2, sizeof(req->buf2), stdin);
	nl = strchr(req->buf2, '\n');
	if (nl != NULL)
		*nl = '\0';

	req->op = outgoing_sms;
	gate_call(smsdgate, 0, 0, 0).call(&gcd, 0);	

	if (rep->err)
		fprintf(stderr, "%s: failed to send sms message (%d)\n", argv[0],
		    rep->err);
	else
		printf("%s: sms message sent\n", argv[0]);

	return (rep->err);
} catch (std::exception &e) {
	printf("rilc: %s\n", e.what());
}
