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

#define RIL_SHLIB
#include "ril/ril.h"

#include "msm_smd.h"
#include "smd_tty.h"
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

int main(int argc, char **argv)
{
    const char *rilLibPath = DEF_RIL_SO;
    char *rilArgv[16];
    void *dlHandle;
    const RIL_RadioFunctions *(*rilInit)(const struct RIL_Env *, int, char **);
    const RIL_RadioFunctions *funcs;
    int rilArgc;

    fprintf(stderr, "Starting smd core\n");
    msm_smd_init();
    smd_tty_init();
    RIL_init();
    sleep(5);

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

    sleep(10);

    fprintf(stderr, "ril versino %d\n", funcs->version);
    fprintf(stderr, "state == %d\n", funcs->onStateRequest());
    //fprintf(stderr, "supports = %x\n", funcs->supports(i));
    fprintf(stderr, "version = %s\n", funcs->getVersion());

    funcs->onRequest(RIL_REQUEST_GET_SIM_STATUS, NULL, 0, (void *)0xdeadbeef);

    for(;;) {
	    fprintf(stderr, "state == %d\n", funcs->onStateRequest());
	sleep(5);
    }

    RIL_register(funcs);

    while(1) {
        // sleep(UINT32_MAX) seems to return immediately on bionic
        sleep(0x00ffffff);
    }
}