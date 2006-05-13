extern "C" {
#include <inc/types.h>
#include <inc/debug.h>

#include <stdio.h>    
#include <stdlib.h>
}

#include <inc/dis/segmessage.hh>
#include <inc/dis/segserver.hh>
#include <inc/scopeguard.hh>

static const char conn_debug = 1;

int
main(int ac, char **av)
{
    if (ac < 2) {
        printf("usage: %s socketfd\n", av[0]);
        exit(-1);
    }
    
    int socket = atoi(av[1]);
    segserver_conn conn(socket);

    segserver_req *req;
    while ((req = conn.next_request())) {
        scope_guard<void, segserver_req *> del_req(delete_obj, req);
        try {
            req->execute();
            const segclient_msg *resp = req->response();
            conn.next_response_is(resp);
        } catch (basic_exception e) {
            debug_print(conn_debug, "unable to execute req: %s", e.what());
            segclient_hdr resp;
            resp.op = segclient_result;
            resp.status = -1;
            resp.psize = 0;
            conn.next_response_is((segclient_msg*)&resp);
        }
    }
}
