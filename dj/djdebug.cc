#include <dj/dis.hh>
#include <dj/djops.hh>

void
dj_debug_delivery(const dj_message_endpoint &endpt,
		  const dj_message_args &a,
		  delivery_status_cb cb)
{
    warn << "dj_debug_delivery: got a message\n";
    warn << a;

    cb(DELIVERY_DONE, 0);
}

void
dj_debug_sink(const dj_message_args &a, uint64_t token)
{
    warn << "dj_debug_sink: running with token " << token << "\n";
    warn << a;
}

void
dj_echo_service(const dj_message_args &a, const str &s, dj_message_args *r)
{
    r->msg = s;
}
