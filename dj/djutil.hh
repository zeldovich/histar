#ifndef JOS_DJ_DJUTIL_HH
#define JOS_DJ_DJUTIL_HH

#include <inc/cpplabel.hh>
#include <dj/gatesender.hh>
#include <dj/djcache.hh>
#include <dj/djprotx.h>

void dj_map_and_delegate(uint64_t lcat, bool integrity,
			 const label &grant_local, const label &grant_remote,
			 uint64_t lct, uint64_t rct, const dj_pubkey &host,
			 gate_sender *gs, dj_global_cache &cache,
			 dj_cat_mapping *lmap,
			 dj_cat_mapping *rmap,
			 dj_stmt_signed *delegation);

#endif
