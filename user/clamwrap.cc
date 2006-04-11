extern "C" {
#include <inc/syscall.h>

#include <stdio.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>

int
main(int ac, char **av)
try
{
    int64_t clam_taint;
    error_check(clam_taint = sys_handle_create());

    
} catch (std::exception &e) {
    printf("clamwrap: %s\n", e.what());
}
