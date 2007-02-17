#ifndef JOS_DJ_DJGATECALL_HH
#define JOS_DJ_DJCATECALL_HH

#include <dj/dis.hh>

class djgate_caller {
 public:
    djgate_caller(cobj_ref djd_gate) { djd_ = djd_gate; }
    dj_reply_status call(str nodepk, dj_gatename gate,
			 const djcall_args &args,
			 djcall_args *resp);

 private:
    djgate_caller(const djgate_caller&);
    djgate_caller &operator=(const djgate_caller&);

    cobj_ref djd_;
};

#endif
