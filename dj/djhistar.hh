#ifndef JOS_DJ_DJHISTAR_HH
#define JOS_DJ_DJHISTAR_HH

#include <dj/stuff.hh>

class histar_token_factory : public token_factory {
 public:
    histar_token_factory() {}
    virtual ~histar_token_factory() {}
    virtual uint64_t token();
};

#endif
