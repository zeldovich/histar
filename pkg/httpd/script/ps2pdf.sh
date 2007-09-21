#!/bin/ksh

/bin/gs -q -dNOPAUSE -dBATCH -sDEVICE=pdfwrite -sOutputFile=$2 -c .setpdfwrite -f $1