/* //device/system/rild/rild.c
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <inc/stdio.h>
#include <inc/gateparam.h>
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/rild.h>

#define RIL_SHLIB
#include "ril/ril.h"

#include "../support/misc.h"
#include "../support/smddgate.h"
}

#include <inc/gatesrv.hh>

#define DEF_RIL_SO	"/bin/libhtc_ril.so"

extern "C" void RIL_register (const RIL_RadioFunctions *callbacks);

extern "C" void RIL_onRequestComplete(RIL_Token t, RIL_Errno e,
                           void *response, size_t responselen);

extern "C" void RIL_onUnsolicitedResponse(int unsolResponse, const void *data,
                                size_t datalen);

extern "C" void RIL_requestTimedCallback (RIL_TimedCallback callback,
                               void *param, const struct timeval *relativeTime);

extern "C" void RIL_issueLocalRequest(int request, void *data, int len);

static struct RIL_Env s_rilEnv = {
	RIL_onRequestComplete,
	RIL_onUnsolicitedResponse,
	RIL_requestTimedCallback
};

extern "C" void RIL_init();
extern "C" void RIL_startEventLoop();

static const RIL_RadioFunctions *funcs;

static void
gate_turn_radio_on()
{
	fprintf(stderr, "TURNING RADIO ON!\n");
	int data = 1; 
	RIL_issueLocalRequest(RIL_REQUEST_RADIO_POWER, &data, sizeof(int));
	sleep(2);

	fprintf(stderr, "STARTING NETWORK SELECTION!\n");
	// Set network selection automatic.
	RIL_issueLocalRequest(RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC, NULL, 0);
}

static void
gate_turn_radio_off()
{
	fprintf(stderr, "TURNING RADIO OFF!\n");
	int data = 0;
	RIL_issueLocalRequest(RIL_REQUEST_RADIO_POWER, &data, sizeof(int));	
}

static void
gate_send_sms(const char *smsstring, int len)
{
	const char *sms[2];

	if (smsstring[len] != '\0') {
		fprintf(stderr, "rild: %s: bad smsstring\n", __func__);
		return;
	}

	sms[0] = NULL;
	sms[1] = strdup(smsstring);		// XXX- never freed!
	fprintf(stderr, "SENDING SMS [%s] [%s]\n", sms[0], sms[1]);
	RIL_issueLocalRequest(RIL_REQUEST_SEND_SMS, sms, sizeof(sms));
}

static void
gate_dial_number(const char *number, int len)
{
	RIL_Dial dialData;

	if (number[len] != '\0') {
		fprintf(stderr, "rild: %s: bad number\n", __func__);
		return;
	}

	dialData.clir = 0;
	dialData.address = strdup(number);	// XXX- never freed!
	fprintf(stderr, "DAILING %s!\n", dialData.address);
	RIL_issueLocalRequest(RIL_REQUEST_DIAL, &dialData, sizeof(dialData));
}

static void
gate_answer_call()
{
	fprintf(stderr, "ANSWERING CALL\n");
	RIL_issueLocalRequest(RIL_REQUEST_ANSWER, NULL, 0);
}

static void
gate_end_call()
{
	int hangupData[1] = {1};

	fprintf(stderr, "ENDING CALL\n");
	RIL_issueLocalRequest(RIL_REQUEST_HANGUP, &hangupData,
		      sizeof(hangupData));
}

static void
gate_default_pdp_on()
{
	// APN, username, password
	fprintf(stderr, "%s: issuing SETUP_DEFAULT_PDP ril request!\n", __func__);
	const char *arg[1] = { "epc.tmobile.com" };
	RIL_issueLocalRequest(RIL_REQUEST_SETUP_DEFAULT_PDP, arg,
		      sizeof(arg));
}

static void
gate_default_pdp_off()
{
	// CID
	fprintf(stderr, "%s: issuing DEACTIVATE_DEFAULT_PDP ril request!\n", __func__);
	const char *arg[1] = { "1" };	//XXX- always the same CID?
	RIL_issueLocalRequest(RIL_REQUEST_DEACTIVATE_DEFAULT_PDP, arg,
		      sizeof(arg));
}

static void
gate_neighboring_cells()
{
	fprintf(stderr, "%s: issuing RIL_REQUEST_GET_NEIGHBORING_CELL_IDS ril "
	    "request!\n", __func__);
	RIL_issueLocalRequest(RIL_REQUEST_GET_NEIGHBORING_CELL_IDS, NULL, 0);
}

static void
gate_registration_status()
{
	fprintf(stderr, "%s: issuing RIL_REQUEST_REGISTRATION_STATE ril request"
	    "\n", __func__);
	RIL_issueLocalRequest(RIL_REQUEST_REGISTRATION_STATE, NULL, 0);
}

static void
rild_dispatch(struct gate_call_data *parm)
{
	struct rild_req *req = (struct rild_req *) &parm->param_buf[0];
	struct rild_reply *reply = (struct rild_reply *) &parm->param_buf[0];

	static_assert(sizeof(*req) <= sizeof(parm->param_buf));
	static_assert(sizeof(*reply) <= sizeof(parm->param_buf));

	int err = 0;
	const char *s;
	switch (req->op) {
	case radio_on:
		gate_turn_radio_on();
		break;

	case radio_off:
		gate_turn_radio_off();
		break;

	case send_sms:
		gate_send_sms(req->buf, req->bufbytes);
		break;

	case dial_number:
		gate_dial_number(req->buf, req->bufbytes);
		break;

	case answer_call:
		gate_answer_call();
		break;

	case end_call:
		gate_end_call();
		break;

	case default_pdp_on:
		gate_default_pdp_on();
		break;

	case default_pdp_off:
		gate_default_pdp_off();
		break;

	case get_ril_interface_version:
		err = funcs->version;
		break;

	case neighboring_cells:
		gate_neighboring_cells();
		break;

	case registration_status:
		gate_registration_status();
		break;

	case get_ril_lib_version:
		s = funcs->getVersion();
		if (s == NULL) {
			err = -E_NOT_FOUND;
		} else {
			strcpy(reply->buf, s);
			reply->bufbytes = strlen(s);
			err = 0;
		}
		break;

	case get_ril_state:
		err = funcs->onStateRequest();
		break;

	default:
		err = -E_INVAL;	
	}

	reply->err = err;
}

static void __attribute__((noreturn))
rild_entry(uint64_t x, struct gate_call_data *parm, gatesrv_return *r)
{
	try {
		rild_dispatch(parm);
	} catch (std::exception &e) {
		printf("%s: %s\n", __func__, e.what());
	}

	try {
		r->ret(0, 0, 0);
	} catch (std::exception &e) {
		printf("%s: ret: %s\n", __func__, e.what());
	}

	thread_halt();
}

int
main(int argc, char **argv)
{
	char *rilArgv[16];
	void *dlHandle;
	const RIL_RadioFunctions *(*rilInit)(const struct RIL_Env *, int, char **);
	int rilArgc;

	// init gates to smdd
	if (smddgate_init()) {
		fprintf(stderr, "rild: smddgate_init failed\n");
		exit(1);
	}

	RIL_init();
	sleep(5);

	dlHandle = dlopen(DEF_RIL_SO, RTLD_NOW);

	if (dlHandle == NULL) {
		fprintf(stderr, "dlopen failed: %s\n", dlerror());
		exit(-1);
	}

	RIL_startEventLoop();

	rilInit = (const RIL_RadioFunctions *(*)(const struct RIL_Env *, int, char **))dlsym(dlHandle, "RIL_Init");

	if (rilInit == NULL) {
		fprintf(stderr, "RIL_Init not defined or exported in %s\n", DEF_RIL_SO);
		exit(-1);
	}

	// Construct the .so's argc/argv 
	rilArgv[0] = argv[0];
	rilArgv[1] = strdup("-d");
	rilArgv[2] = strdup("qmd0");
	rilArgv[3] = NULL;
	rilArgc = 3;

	fprintf(stderr, "Calling rilInit @ %p\n", rilInit);
	funcs = rilInit(&s_rilEnv, rilArgc, rilArgv);
	fprintf(stderr, "rilInit returned\n");

	RIL_register(funcs);

	// init our gate
	struct cobj_ref g = gate_create(start_env->root_container,
	    "rild gate", 0, 0, 0, &rild_entry, 0);

	while(1) {
		// sleep(UINT32_MAX) seems to return immediately on bionic
		sleep(0x00ffffff);
	}
}
