#include <lib/vt/vtlexgl.h>
#include <lib/vt/lex.h>
#include <lib/vt/vt.h>
#include <inc/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
    //char buf[MAX_ESC] ;
    //sprintf(buf, "%s(%d)\n", s, n) ;
    //cons_puts(buf, strlen(buf)) ;
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
vt_handle_RCUR(const char *text, int n)
{
    vt_debug("RCUR", n) ;   
}

void
vt_handle_SCUR(const char *text, int n)
{
    vt_debug("SCUR", n) ;   
}

void
vt_handle_CUP(const char *text, int n)
{
    char buf[32] ;
    memcpy(buf, &text[2], n - 3) ;
    buf[n - 3] = 0 ;
    char *bk = index(buf, ';') ;
    *bk = 0 ;
    int line = atoi(buf) ;
    int col = atoi(bk + 1) ;
    
    vt_debug("CUP", n) ;   
    // vt origin (1,1)
    sys_cons_cursor(line - 1, col - 1) ;
}

void
vt_handle_ED(const char *text, int n)
{
    vt_debug("ED", n) ;   
}

void
vt_handle_SGR(const char *text, int n)
{
    vt_debug("SGR", n) ;   
}

void
vt_handle_STR(const char *text, int n)
{
    cons_puts(text, n);
}

int
vt_write(const void *vbuf, size_t n, off_t offset)
{
    char *buf = malloc(n + 1) ;
    memcpy(buf, vbuf, n) ;
    buf[n] = 0 ;
    
    struct yy_buffer_state* bs = vtscan_string(buf) ;
    vtswitch_buffer(bs) ;
    vtlex() ; 
   
    return 0 ;
}

