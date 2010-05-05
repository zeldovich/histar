#!/bin/sh
if [ "$1" == "" ]; then
	echo "usage: $0 uW"
	exit 1
fi
energywrap $1 /bin/spin.sh &
energywrap $1 /bin/spin.sh
