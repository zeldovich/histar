#include <lib/vt/vtlexgl.h>
#include <lib/vt/lex.h>
#include <lib/vt/vt.h>
#include <inc/syscall.h>
#include <stdio.h>
#include <string.h>

// http://www.vt100.net/docs/vt100-ug/chapter3.html

#define MAX_ESC 32

static void
cons_puts(const char *vbuf, int n)
{
    sys_cons_puts(vbuf, n);      
}

static void
vt_debug(const char *s, int n)
{
   char buf[MAX_ESC] ;
    
    sprintf(buf, "%s(%d)\n", s, n) ;
    cons_puts(buf, strlen(buf)) ;
}   

void
vt_handle_ESC(const char *text, int n)
{
    vt_debug("ESC", n) ;
}

void
vt_handle_CSI(const char *text, int n)
{
    vt_debug("CSI", n) ;
}

void 
vt_handle_SCS(const char *text, int n)
{
    vt_debug("SCS", n) ;   
}

void
vt_handle_SCUR(const char *text, int n)
{
    vt_debug("SCUR", n) ;   
}

void
vt_handle_CUP(const char *text, int n)
{
    vt_debug("CUP", n) ;   
}

void
vt_handle_STR(const char *text, int n)
{
    cons_puts(text, n);
}

int
vt_write(const void *vbuf, size_t n, off_t offset)
{
    int fd[2] ;
    int r ;
    FILE *f ;
    
    if ((r = pipe(fd)) < 0)
        return r ;
    
    f = fdopen(fd[0], "r") ;
    if (f == 0)
        return -1 ;
    vtlexin_is(f) ;
    vtlexout_is(stdout) ;

    write(fd[1], vbuf, n) ;
    if ((r = close(fd[1])) < 0)
        printf("close: %d\n", r)  ;
    
    vtlex() ;
    
    close(fd[0]) ;
    
    return 0 ;
}


/*
static const char test[] = "\033)0\0337NORMAL\033[1;1HThis is a normal string" ;


// test main
int
main(int ac, char **av)
{
    const char *t ;
    
    if (ac == 2)
        t = av[1] ;
    else
        t = test ;

    if (vt_write(t, strlen(t), 0) < 0)
        printf("vt_write: test error\n") ;
    return 0 ;
}*/

