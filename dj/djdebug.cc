#include <dj/djprot.hh>
#include <dj/djops.hh>

void
dj_debug_delivery(const dj_esign_pubkey &sender,
		  const dj_message &a,
		  delivery_status_cb cb)
{
    warn << "dj_debug_delivery: got a message from " << sender << "\n";
    warn << a;

    cb(DELIVERY_DONE, 0);
}
