extern "C" {
#include <inc/types.h> 
#include <inc/lib.h>   
#include <inc/syscall.h>
#include <inc/gateparam.h>   

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
}

#include <inc/dis/importc.hh>
#include <inc/dis/exportd.hh>  // for seg_stat
#include <inc/dis/caclient.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>
#include <inc/gateclnt.hh>

int 
main (int ac, char **av)
{
    static const char path[] = "/tmp/test_file";
    static char buffer[128];

    try {
        int64_t export_ct, manager_gt, wrap_ct, wrap_gt;
        error_check(export_ct = container_find(start_env->root_container, kobj_container, "importd"));
        error_check(manager_gt = container_find(export_ct, kobj_gate, "manager"));
        error_check(wrap_ct = container_find(export_ct, kobj_container, "default_wrap"));
        error_check(wrap_gt = container_find(wrap_ct, kobj_gate, "gate"));

                
        import_managerc manager(COBJ(export_ct, manager_gt), COBJ(wrap_ct, wrap_gt));
        import_segmentc *seg = manager.segment_new(path);
        
        int r = seg->read(buffer, sizeof(buffer), 0);
        printf("read r %d\n", r);
        for (int i = 0; i < r; i++)
            printf("%c", buffer[i]);        
        printf("\n");
        
        static const char test_reply[] = "remote write?!?";
        strcpy(buffer, test_reply);
        r = seg->write(buffer, strlen(test_reply), 10);
        printf("write r %d\n", r);
        
        r = seg->read(buffer, sizeof(buffer), 0);
        printf("read r %d\n", r);
        for (int i = 0; i < r; i++)
            printf("%c", buffer[i]);        
        printf("\n");
        
        //struct seg_stat ss;
        //seg->stat(&ss);
        //printf("stat ss.ss_size %ld\n", ss.ss_size);
        
        printf("test done!\n");
        manager.segment_del(seg);
        return 0;
    } catch (basic_exception e) {
        printf("main: %s\n", e.what());
        exit(-1);           
    }
}
