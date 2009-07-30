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

#define RIL_SHLIB
#include "ril/ril.h"

#include "msm_smd.h"
#include "smd_tty.h"
#include "smd_qmi.h"
#include "smd_rpcrouter.h"
#include "msm_rpcrouter.h"
#include "msm_rpcrouter2.h"
#include "gps.h"

#include "support/misc.h"
}

#define DEF_RIL_SO	"/bin/libhtc_ril.so"

extern "C" void RIL_register (const RIL_RadioFunctions *callbacks);

extern "C" void RIL_onRequestComplete(RIL_Token t, RIL_Errno e,
                           void *response, size_t responselen);

extern "C" void RIL_onUnsolicitedResponse(int unsolResponse, const void *data,
                                size_t datalen);

extern "C" void RIL_requestTimedCallback (RIL_TimedCallback callback,
                               void *param, const struct timeval *relativeTime);

static struct RIL_Env s_rilEnv = {
    RIL_onRequestComplete,
    RIL_onUnsolicitedResponse,
    RIL_requestTimedCallback
};

extern "C" void RIL_init();
extern "C" void RIL_startEventLoop();

static int handle_battery_call(struct msm_rpc_server *server,
    struct rpc_request_hdr *req, unsigned len)
{
	printf("%s: prog %x, vers %u, procedure %d\n", __func__, req->prog, req->vers, req->procedure);
	return (0);
}

static void
location_callback(GpsLocation *location)
{
	fprintf(stderr, "::%s::\n", __func__);
	fprintf(stderr, "  latitude:  %f\n", location->latitude);
	fprintf(stderr, "  longitude: %f\n", location->latitude);
}

static void
status_callback(GpsStatus *status)
{
	fprintf(stderr, "::%s::\n", __func__);
	fprintf(stderr, "  status: %d\n", status->status);
}

static void
sv_status_callback(GpsSvStatus *sv_info)
{
	fprintf(stderr, "::%s::\n", __func__);
	fprintf(stderr, "  number of SVs:  %d\n", sv_info->num_svs);
}

static void download_request_callback() { fprintf(stderr, "--Xtra download request CB\n"); }

GpsXtraCallbacks sGpsXtraCallbacks = {
    download_request_callback,
};

int main(int argc, char **argv)
{
    const char *rilLibPath = DEF_RIL_SO;
    char *rilArgv[16];
    void *dlHandle;
    const RIL_RadioFunctions *(*rilInit)(const struct RIL_Env *, int, char **);
    const RIL_RadioFunctions *funcs;
    const GpsInterface *(*gps_get_hardware_interface)(void);
    int rilArgc;

    fprintf(stderr, "Starting smd core\n");
    msm_smd_init();
    smd_tty_init();
    smd_qmi_init();
    smd_rpcrouter_init();
    smd_rpc_servers_init();
    RIL_init();
    sleep(5);

static struct msm_rpc_server battery_server;

battery_server.prog = 0x30100000,
battery_server.vers = 0,
battery_server.rpc_call = handle_battery_call;
msm_rpc_create_server(&battery_server);
struct msm_rpc_endpoint *endpt = msm_rpc_connect(0x30100001, 0, 0);
if (IS_ERR(endpt)) { fprintf(stderr, "msm_rpc_connect failed: %d (%s)\n", PTR_ERR(endpt), e2s(PTR_ERR(endpt))); exit(1); }

struct rpc_request_hdr req;
struct htc_get_batt_info_rep {
	struct rpc_reply_hdr hdr; 
	struct battery_info_reply {
		uint32_t batt_id;            /* Battery ID from ADC */
		uint32_t batt_vol;           /* Battery voltage from ADC */
		uint32_t batt_temp;          /* Battery Temperature (C) from formula and ADC */
		uint32_t batt_current;       /* Battery current from ADC */
		uint32_t level;              /* formula */
		uint32_t charging_source;    /* 0: no cable, 1:usb, 2:AC */
		uint32_t charging_enabled;   /* 0: Disable, 1: Enable */
		uint32_t full_bat;           /* Full capacity of battery (mAh) */
	} info;
} rep;

while (1) {
int rc = msm_rpc_call_reply(endpt, 2, &req, sizeof(req), &rep, sizeof(rep), 5000);
if (rc < 0) {
	fprintf(stderr, "msm_rpc_call_reply failed: %d\n", rc);
} else {
	printf("batt_id:      %u\n", be32_to_cpu(rep.info.batt_id));
	printf("batt_vol:     %u\n", be32_to_cpu(rep.info.batt_vol));
	printf("batt_temp:    %u\n", be32_to_cpu(rep.info.batt_temp));
	printf("batt_current: %u\n", be32_to_cpu(rep.info.batt_current));
	printf("level:        %u\n", be32_to_cpu(rep.info.level));
	printf("charging src: %u\n", be32_to_cpu(rep.info.charging_source));
	printf("charging ena: %u\n", be32_to_cpu(rep.info.charging_enabled));
	printf("full capacity: %u\n", be32_to_cpu(rep.info.full_bat));
}
}

    dlHandle = dlopen("/bin/libgps.so", RTLD_NOW);
    if (dlHandle == NULL) {
	fprintf(stderr, "dlopen failed: %s\n", dlerror());
	exit(-1);
    }
    fprintf(stderr, "libgps.so loaded.\n");

    gps_get_hardware_interface = (const GpsInterface *(*)(void))dlsym(dlHandle, "gps_get_hardware_interface");
    if (gps_get_hardware_interface == NULL) {
	fprintf(stderr, "gps_get_hardware_interface not defined or exported in %s\n", "/bin/libgps.so");
	exit(1);
    }

    fprintf(stderr, "gps_get_hardware_interface at %p\n", gps_get_hardware_interface);
    fprintf(stderr, "Calling gps_get_hardware_interface()\n");
    const GpsInterface *iface = gps_get_hardware_interface();
    fprintf(stderr, "GpsInterface at %p, iface->init at %p, iface->start at %p, iface->stop at %p\n", iface, iface->init, iface->start, iface->stop);
    GpsCallbacks cbs = { location_callback, status_callback, sv_status_callback };
    iface->init(&cbs);
    fprintf(stderr, "GpsInit done.\n");
    const GpsXtraInterface *xtra = (const GpsXtraInterface*)iface->get_extension(GPS_XTRA_INTERFACE);
    if (xtra != NULL) {
	fprintf(stderr, "Gps supports Xtra interface\n");
	int r = xtra->init(&sGpsXtraCallbacks);
	if (r)
		fprintf(stderr, " -- Xtra interface init failed: %d\n", r);
    } else {
	fprintf(stderr, "Xtra interface not supported\n");
    }
    iface->delete_aiding_data(GPS_DELETE_ALL);
    iface->set_position_mode(GPS_POSITION_MODE_STANDALONE, 10);
start:
    iface->start();

    sleep(30);
    fprintf(stderr, "Stopping interface\n");
    iface->stop();
    sleep(25);
    fprintf(stderr, "Restarting interface\n");
    goto start;

    fprintf(stderr, "opening NMEA tty\n");
    smd_tty_open(27); 

    while (1) {
	unsigned char buf[512];
	smd_tty_read(27, buf, sizeof(buf));
	fprintf(stderr, "NMEA: [%s]\n", buf);
    }

sleep(9999);

    dlHandle = dlopen(rilLibPath, RTLD_NOW);

    if (dlHandle == NULL) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        exit(-1);
    }

    RIL_startEventLoop();

    rilInit = (const RIL_RadioFunctions *(*)(const struct RIL_Env *, int, char **))dlsym(dlHandle, "RIL_Init");

    if (rilInit == NULL) {
        fprintf(stderr, "RIL_Init not defined or exported in %s\n", rilLibPath);
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

    sleep(20);

    fprintf(stderr, "ril versino %d\n", funcs->version);
    fprintf(stderr, "state == %d\n", funcs->onStateRequest());
    //fprintf(stderr, "supports = %x\n", funcs->supports(i));
    fprintf(stderr, "version = %s\n", funcs->getVersion());

    for(;;) {
	    fprintf(stderr, "state == %d\n", funcs->onStateRequest());
	sleep(5);
    }

    while(1) {
        // sleep(UINT32_MAX) seems to return immediately on bionic
        sleep(0x00ffffff);
    }
}
