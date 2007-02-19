#include <dj/djprot.hh>
#include <dj/djops.hh>

void
dj_debug_delivery(const dj_pubkey &sender,
		  const dj_message &a,
		  delivery_status_cb cb)
{
    warn << "dj_debug_delivery: got a message from " << sender << "\n";
    warn << a;

    cb(DELIVERY_DONE, 0);
}

void
dj_debug_sink(const dj_pubkey &sender, const dj_message &a,
	      uint64_t selftoken)
{
    warn << "dj_debug_sink: got a message from " << sender
	 << ", selftoken " << selftoken << "\n";
    warn << a;
}
