extern "C" {
#include <stdio.h>    
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/remfile.h>
#include <inc/error.h>
#include <string.h>
#include <stdio.h>
#include <inc/debug.h>
}

#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>

#include <lib/dis/fileserver.hh>

static void __attribute__((noreturn))
export_srv(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
try {
    gr->ret(0, 0, 0);
}
catch (std::exception &e) {
    printf("export_srv: %s\n", e.what());
    gr->ret(0, 0, 0);
}

void 
exportsrv_start(uint64_t container, label *la, 
                label *clearance)
{
    gate_create(container,"export server", la, 
                    clearance, &export_srv, 0);
    
    fileserver_start(8080);
}

