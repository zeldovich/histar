#!/bin/sh
if [ "$1" == "" ]; then
	"usage: $0 uW"
	exit 1
echo
energywrap $1 /bin/rsstest.sh &
energywrap $1 /bin/mmtest.sh &
echo "master energywrap parent sleeping..."
sleep 10000
