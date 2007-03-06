#ifndef JOS_DJ_DJDEBUG_HH
#define JOS_DJ_DJDEBUG_HH

void dj_debug_delivery(const dj_pubkey &sender,
		       const dj_message &a,
		       delivery_status_cb cb);

void dj_debug_sink(const dj_pubkey &sender,
		   const dj_message &a);

#endif
