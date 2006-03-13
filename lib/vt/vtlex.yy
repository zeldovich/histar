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
		/* select character set */
SCS		{ESC}(\(|\))[AB012]

/* Control Sequences */
		/* cursor position */
CUP		{CSI}{PN};{PN}H

/* ANSI Compatibility */
		/* save cursor attributes */
SCUR		{ESC}7

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
