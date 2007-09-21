#include <dj/djprot.hh>
#include <dj/djops.hh>
#include <dj/djrpc.hh>

void
dj_debug_delivery(const dj_message &a,
		  delivery_status_cb cb)
{
    warn << "dj_debug_delivery: got a message\n";
    warn << a;

    cb(DELIVERY_DONE);
}

void
dj_debug_sink(const dj_message &a)
{
    warn << "dj_debug_sink: got a message\n";
    warn << a;
}

bool
dj_echo_service(const dj_message &m, const str &s, dj_rpc_reply *r)
{
    r->msg.msg = s;
    return true;
}
