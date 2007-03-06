#ifndef JOS_DJ_DELEGATOR_HH
#define JOS_DJ_DELEGATOR_HH

#include <dj/djprot.hh>
#include <dj/stuff.hh>

void delegation_create(djprot*, const dj_pubkey&,
		       const dj_message&, const delivery_args&);

#endif
