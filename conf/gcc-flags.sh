#!/bin/sh
CC="$1"
CCOPT="$2"
if echo | $CC - -E -o /dev/null $CCOPT 2>/dev/null; then
    echo $CCOPT
fi
