#!/bin/sh
CCOPTS="-fgnu89-inline -fno-stack-protector"
CC="$1"
for CCOPT in $CCOPTS; do
    if echo | $CC - -E -o /dev/null $CCOPT 2>/dev/null; then
	echo $CCOPT
    fi
done
