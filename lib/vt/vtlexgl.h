#ifndef VTLEXGL_H_
#define VTLEXGL_H_

void vt_handle_ESC(const char *text, int n) ;
void vt_handle_CSI(const char *text, int n) ;
void vt_handle_SCS(const char *text, int n) ;
void vt_handle_SCUR(const char *text, int n) ;
void vt_handle_CUP(const char *text, int n) ;
void vt_handle_STR(const char *text, int n) ;

#define HANDLE(_T) vt_handle_##_T(yytext, yyleng)

#endif /*VTLEXGL_H_*/
