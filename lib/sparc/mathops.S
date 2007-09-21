#include <machine/asm.h>

ENTRY(.umul)
	umul	%o0, %o1, %o0
	retl
	 rd	%y, %o1

ENTRY(.udiv)
	wr	%g0, 0, %y
	nop
	nop
	retl
	 udiv	%o0, %o1, %o0

ENTRY(.div)
	sra	%o0, 31, %o2
	wr	%o2, 0, %y
	nop
	nop
	nop
	sdivcc	%o0, %o1, %o0
	bvs,a	1f
	 xnor	%o0, %g0, %o0
1:	retl
	 nop

ENTRY(.urem)
	wr	%g0, 0, %y
	nop
	nop
	nop
	udiv	%o0, %o1, %o2
	umul	%o2, %o1, %o2
	retl
	 sub	%o0, %o2, %o0

ENTRY(.rem)
	sra	%o0, 31, %o2
	wr	%o2, 0, %y
	nop
	nop
	nop
	sdivcc	%o0, %o1, %o2
	bvs,a	1f
	 xnor	%o2, %g0, %o2
1:	smul	%o2, %o1, %o2
	retl
	 sub	%o0, %o2, %o0
