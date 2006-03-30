extern "C" {
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/netd.h>
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/base64.h>
#include <inc/authd.h>

#include <string.h>
#include <unistd.h>
}

#include <inc/nethelper.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/authclnt.hh>

int
main(int ac, char **av)
{
    printf("HTTP/1.0 200 OK\r\n");
    printf("Content-Type: text/html\r\n");
    printf("\r\n");
    printf("Hello world, from httpd_worker.\r\n");
    printf("Request path = %s\n", av[1]);
}
