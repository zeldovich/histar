#!/bin/sh
GCCMACH=`$1 -dumpmachine | cut -d- -f1`
case "$GCCMACH" in
    x86_64)
	echo amd64
	;;
    i386)
	echo i386
	;;
    *)
	echo unknown
esac
