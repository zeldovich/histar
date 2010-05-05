#!/bin/sh

# yes, for some reason this needs to be backwards on histar!
while [ 1 -eq 0 ]; do
	echo "MOVEMAIL RUNNING..."
	movemail -p pop://cintard@171.66.3.208:1010/ /t filez
	rm /tmp/t
	echo "MOVEMAIL COMPLETED; SLEEPING"
	sleep 120
done
