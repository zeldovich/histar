%{
#include <lib/vt/vtlexgl.h>
#include <lib/vt/lex.h>
%}

/* Definitions */
ESC		\033
CSI		{ESC}\[
PN		[0-9]+
STR		[^\033]*

/* Escape Sequences */
SCS		{ESC}(\(|\))[AB012]	/* select character set */

/* Control Sequences */
CUP		{CSI}{PN};{PN}H	/* cursor position */

/* ANSI Compatibility */
SCUR	{ESC}7			/* save cursor attributes */

%%

{ESC}		HANDLE(ESC) ;
{CSI}		HANDLE(CSI) ;

{SCS}		HANDLE(SCS) ;

{SCUR}		HANDLE(SCUR) ;

{CUP}		HANDLE(CUP) ;

{STR}		HANDLE(STR) ;	

%%

int
yywrap(void)
{
	return 1 ;
}