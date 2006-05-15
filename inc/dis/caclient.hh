#ifndef SECRET_H_
#define SECRET_H_

struct sign_key 
{
    uint64_t xxx;
};

class sign_agent 
{
public:
    sign_agent(cobj_ref gate) : gate_(gate) {}

    void sign(const void *subject, int n, void *sign, int m) {}

private:
    cobj_ref gate_;
};

class ver_agent 
{
public:    
    ver_agent(cobj_ref gate) : gate_(gate) {}

    void     verify(const void *subject, int n, void *sign, int m);
    cobj_ref taint(cobj_ref seg);

private:
    cobj_ref gate_;
};

class auth_agent
{
public:
    auth_agent(cobj_ref sign, cobj_ref ver) :
        sign_(sign), ver_(ver) {}
    auth_agent(const char *name);
    
    sign_agent sign_agent_new(const char *name, uint64_t grant);    
    ver_agent  ver_agent_new(const char *name, uint64_t grant);    

private:
    cobj_ref sign_;
    cobj_ref ver_;
};

#endif /*SECRET_H_*/
