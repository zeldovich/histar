#ifndef SECRET_H_
#define SECRET_H_

struct sign_key 
{
    uint64_t xxx;
};

struct ca_response
{
    
};

cobj_ref ca_new(const char *name);
char ca_auth(ca_response *res);


#endif /*SECRET_H_*/
