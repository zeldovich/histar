%{
#include <lib/vt/vtlexgl.h>
#include <lib/vt/lex.h>
%}

/* Definitions */
ESC		\033
CSI		{ESC}\[
PN		[0-9]+
PS		[0-9]
STR		[^\033]*

/* Escape Sequences */
		/* select character set */
SCS		{ESC}(\(|\))[AB012]

/* Control Sequences */
		/* cursor position */
CUP		{CSI}{PN};{PN}H
		/* erase in display */
ED		{CSI}{PS}J
		/* select graphic rendition */
SGR		{CSI}{PS}(;{PS})*m

/* ANSI Compatibility */
		/* save cursor and attributes */
SCUR		{ESC}7
		/* restor cursor and attributes */
RCUR		{ESC}8

%%

{ESC}		HANDLE(ESC) ;
{CSI}		HANDLE(CSI) ;

{SCS}		HANDLE(SCS) ;

{SCUR}		HANDLE(SCUR) ;
{RCUR}		HANDLE(RCUR) ;

{CUP}		HANDLE(CUP) ;
{ED}		HANDLE(ED) ;
{SGR}		HANDLE(SGR) ;

{STR}		HANDLE(STR) ;	

%%

int
yywrap(void)
{
	return 1 ;
}
