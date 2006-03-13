#ifndef LEX_H_
#define LEX_H_

#include <stdio.h>

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

int yylex(void) ;

#define YY_DECL int vt_lex(void)
YY_DECL ;

#endif /*LEX_H_*/
