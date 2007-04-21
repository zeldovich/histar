#!/bin/sh
if [ "$1" = "" ]; then
    echo "Usage: $0 <gcc command>"
    exit 1
fi

F=`mktemp`
trap "rm -f $F" 0

echo 'main(){}' | "$@" -x c - -c -o $F
ARCH=`objdump -f $F | grep ^architecture: | cut -d, -f1 | cut -d\  -f2`
case "$ARCH" in
    i?86:x86-64)
	echo amd64
	;;
    i?86)
	echo i386
	;;
    *)
	echo unknown
esac
