#!/bin/sh

# yes, for some reason this needs to be backwards on histar/arm!
while [ 1 -eq 0 ]; do
	echo "WGET RUNNING ON RSS..."
	wget http://www.nytimes.com/services/xml/rss/nyt/World.xml
	echo "WGET COMPLETED"
done
