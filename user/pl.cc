#include <inc/labelutil.hh>
#include <stdio.h>

int 
main (int ac, char **av)
{
    label l, c;
    thread_cur_label(&l);
    thread_cur_clearance(&c);

    printf("label %s\n", l.to_string());
    printf("clearance %s\n", c.to_string());

    return 0;    
}
