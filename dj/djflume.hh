#ifndef JOS_DJ_DJFLUME_HH
#define JOS_DJ_DJFLUME_HH

#include <dj/djprot.hh>
#include <dj/djarpc.hh>

class flume_mapcreate {
 public:
    flume_mapcreate(djprot *p) : p_(p) {}
    void exec(const dj_message&, const delivery_args&);

 private:
    djprot *p_;
};

void dj_flume_perl_svc(flume_mapcreate *,
		       const dj_message&, const str&,
		       const dj_arpc_reply&);

void dj_flume_ctalloc_svc(flume_mapcreate *,
			  const dj_message&, const str&,
			  const dj_arpc_reply&);

#endif
