extern "C" {
#include <inc/types.h> 
#include <inc/lib.h>   
#include <inc/syscall.h>
#include <inc/gateparam.h>   

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
}

#include <inc/dis/globalcatc.hh>
#include <inc/dis/globalcatd.hh>
#include <inc/dis/globallabel.hh>

#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>
#include <inc/gateclnt.hh>

int 
main (int ac, char **av)
{
    int64_t gcd_ct, gcd_gt;
    error_check(gcd_ct = container_find(start_env->root_container, kobj_container, "globalcatd"));
    error_check(gcd_gt = container_find(gcd_ct, kobj_gate, "globalcat srv"));
    
    uint64_t export_grant = handle_alloc();
    
    global_catc gc(COBJ(gcd_ct, gcd_gt), export_grant);
    uint64_t h0 = handle_alloc();
    printf("h0 %ld\n", h0);
    gc.global_is(h0, "handle 0");

    uint64_t h0_l = gc.local("handle 0", true);
    printf("h0_l %ld\n", h0_l);

    label l(1);
    l.set(h0, 2);
    global_label gl(&l);
    
    printf("global label %s\n", gl.string_rep());
    return 0;    
}
