#ifndef LEX_H_
#define LEX_H_

#include <stdio.h>

extern FILE *yyin ;
extern FILE *yyout ;

// flex is janky, avoid diffs between flex versions
#define vtlexin_is(_F) (yyin = f)
#define vtlexout_is(_F) (yyout = f)
#define vtscan_string(_BUF) josvt_yy_scan_string(_BUF)
#define vtswitch_buffer(_BUF) josvt_yy_switch_to_buffer(_BUF)
#define YY_DECL int vtlex(void)
YY_DECL ;

// get rid of warnings
struct yy_buffer_state ;
int yyget_lineno  (void) ;
FILE *yyget_in  (void) ;
FILE *yyget_out  (void) ;
int yyget_leng  (void) ;
char *yyget_text  (void) ;

void yyset_lineno (int  line_number ) ;
void yyset_in (FILE *  in_str ) ;
void yyset_out (FILE *  out_str ) ;
int yyget_debug  (void) ;
void yyset_debug (int  bdebug ) ;
int yylex_destroy  (void) ;
struct yy_buffer_state *yy_scan_string(const char *str) ;
void yy_switch_to_buffer(struct yy_buffer_state *bs);

#endif /*LEX_H_*/
