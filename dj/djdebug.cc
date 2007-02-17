#include <dj/dis.hh>
#include <dj/djops.hh>

void
dj_debug_delivery(const dj_message_endpoint &endpt,
		  const dj_message_args &a,
		  delivery_status_cb cb)
{
    warn << "dj_debug_delivery: got a message\n";
    warn << "container: " << a.msg_ct << "\n";
    warn << "sent token: " << a.token << "\n";
    warn << "named categories:";
    for (uint32_t i = 0; i < a.namedcats.size(); i++)
	warn << " " << a.namedcats[i];
    warn << "\n";
    warn << "taint: " << a.taint << "\n";
    warn << "grant label: " << a.glabel << "\n";
    warn << "grant clear: " << a.gclear << "\n";
    warn << "payload: " << a.msg << "\n";

    cb(DELIVERY_DONE, 0);
}

void
dj_debug_sink(const dj_message_args &a, uint64_t token)
{
    warn << "dj_debug_sink: running with token " << token << "\n";
    warn << "container: " << a.msg_ct << "\n";
    warn << "sent token: " << a.token << "\n";
    warn << "named categories:";
    for (uint32_t i = 0; i < a.namedcats.size(); i++)
	warn << " " << a.namedcats[i];
    warn << "\n";
    warn << "taint: " << a.taint << "\n";
    warn << "grant label: " << a.glabel << "\n";
    warn << "grant clear: " << a.gclear << "\n";
    warn << "payload: " << a.msg << "\n";
}
