#ifndef JOS_DJ_DJDEBUG_HH
#define JOS_DJ_DJDEBUG_HH

void dj_debug_delivery(const dj_esign_pubkey &sender,
		       const dj_message &a,
		       delivery_status_cb cb);

#endif
