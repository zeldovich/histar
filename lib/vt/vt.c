#include <lib/vt/vtlexgl.h>
#include <lib/vt/lex.h>
#include <lib/vt/vt.h>
#include <stdio.h>
#include <string.h>

// not thread safe, but will be replaced by appropriate system
// calls...
// http://www.vt100.net/docs/vt100-ug/chapter3.html

#define MAX_ESC 32
static char scratch[MAX_ESC] ;

static void
cons_puts(const char *vbuf, int n)
{
    sys_cons_puts(vbuf, n);      
}

void
vt_handle_ESC(const char *text, int n)
{
    sprintf(scratch, "ESC(%d)\n", n) ;
    cons_puts(scratch, strlen(scratch)) ;
}

void
vt_handle_CSI(const char *text, int n)
{
    sprintf(scratch, "CSI(%d)\n", n) ;
    cons_puts(scratch, strlen(scratch)) ;
}

void 
vt_handle_SCS(const char *text, int n)
{
    sprintf(scratch, "SCS(%d)\n", n) ;   
    cons_puts(scratch, strlen(scratch)) ;
}

void
vt_handle_SCUR(const char *text, int n)
{
    sprintf(scratch, "SCUR(%d)\n", n) ;   
    cons_puts(scratch, strlen(scratch)) ;
}

void
vt_handle_CUP(const char *text, int n)
{
    sprintf(scratch, "CUP(%d)\n", n) ;   
    cons_puts(scratch, strlen(scratch)) ;
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
    yyset_in(f) ;
    yyset_out(stdout) ;

    write(fd[1], vbuf, n) ;
    if ((r = close(fd[1])) < 0)
        printf("close: %d\n", r)  ;
    
    vt_lex() ;
    
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

