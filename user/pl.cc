extern "C" {
#include <inc/authd.h>
}

#include <inc/labelutil.hh>
#include <stdio.h>
#include <inc/authclnt.hh>

int 
main (int ac, char **av)
{
    label l, c;
    thread_cur_label(&l);
    thread_cur_clearance(&c);

    printf("label %s\n", l.to_string());
    printf("clearance %s\n", c.to_string());
    
    authd_reply reply;
    auth_call(authd_getuid, "", "", "", &reply);
    
    printf("uid %ld\n", reply.user_id);

    return 0;    
}
