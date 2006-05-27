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
    static const char host[] = "127.0.0.1";
    static const uint16_t port = 9999;
    static const char path[] = "/tmp/test_file";
    static char buffer[128];

    try {
        /*
        
        
        auth_agent aa = auth_agent("cad");
        sign_agent sa = aa.sign_agent_new("bob signer", grant);
        ver_agent va = aa.ver_agent_new("server ver", grant);
        //sa.secret_is("bob", "some secret", strlen("some secret"));
        
        va.verify(0, 0, 0, 0);

        struct cobj_ref test_seg;
        error_check(segment_alloc(start_env->shared_container, 10,
                         &test_seg, 0, 0, "test"));
        va.taint(test_seg);
        */
        
        int64_t export_ct, manager_gt;
        error_check(export_ct = container_find(start_env->root_container, kobj_container, "exportd"));
        error_check(manager_gt = container_find(export_ct, kobj_gate, "manager"));
        uint64_t grant = handle_alloc();
                
        import_managerc manager(COBJ(export_ct, manager_gt));
        //export_clientc client = manager.client_new((char*)"bob", grant);
        //export_segmentc seg = client.segment_new(host, port, path);

        import_segmentc seg = manager.segment_new(host, port, path, grant);
       
        int r = seg.read(buffer, sizeof(buffer), 0);
        printf("read r %d\n", r);
        for (int i = 0; i < r; i++)
            printf("%c", buffer[i]);        
        printf("\n");
        
        static const char test_reply[] = "remote write?!?";
        strcpy(buffer, test_reply);
        r = seg.write(buffer, strlen(test_reply), 10);
        printf("write r %d\n", r);
        
        struct seg_stat ss;
        seg.stat(&ss);
        printf("stat ss.ss_size %ld\n", ss.ss_size);
        
        printf("test done!\n");
        manager.segment_del(&seg);
        return 0;
    } catch (basic_exception e) {
        printf("main: %s\n", e.what());
        exit(-1);           
    }
}
