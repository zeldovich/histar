#!/bin/sh
# host-file contains ip address of usable hosts.  adm-file contains
# IPMI ip addresses for all the hosts in host-file.  results and
# temporary files are placed in results-dir.  Note the first hosts
# in host-file will be assigned to httpd hosts, so set $httpdip 
# accordingly.

setup=./dj-httpd-setup.sh
httpdtest=../obj/test/httpd-test
httpdip=171.66.3.200
numruns=3

if [ $# -ne "3" ]
then
    echo "usage: $0 host-file adm-file results-dir"
    exit 1
fi

hostlist=$1
admlist=$2
dir=$3

function fillfile {
    local file=$1
    local offset=$2
    local count=$3

    local i=0;
    while [ $i -lt $count ]
      do
      line=$(( $i + $offset ))
      ip=$( sed -n "$line,${line}p" $hostlist )
      echo "$ip" >> $dir/$file
      i=$(( $i + 1))
    done
}

function benchmark {
    local httpd=$1
    local app=$2
    local user=$3
    local name=$4
    local benchcmd=$5
    echo "Benchmarking $httpd httpd $app app $user user ($name)..."

    rm -f $dir/httpd.tmp
    rm -f $dir/app.tmp
    rm -f $dir/user.tmp
    fillfile "httpd.tmp" 1 $httpd
    fillfile "app.tmp" $(( 1 + $httpd )) $app
    fillfile "user.tmp" $(( 1 + $httpd + $app)) $user

    local out=$dir/$httpd-$app-$user-$name.out
    rm -f $out
    echo "Running setup script..."
    $setup $admlist $dir/httpd.tmp $dir/app.tmp $dir/user.tmp > $out

    echo "Running: $benchcmd"
    echo "--------------" >> $out
    echo "|BENCH OUTPUT|" >> $out
    echo "--------------" >> $out
    echo -n "start: " >> $out
    cat /proc/uptime >> $out
    $benchcmd >> $out 2>&1
    echo -n "end: " >> $out
    cat /proc/uptime >> $out
    echo "Benchmark $httpd httpd $app app $user user ($name) done!"
}

function dotp {
    local httpd=$1
    local app=$2
    local user=$3
    local clientmin=$4
    local clientmax=$5
    local req=$6

    local i=$clientmin
    while [ $i -le $clientmax ]
      do
      local j=0
      while [ $j -lt $numruns ]
	do
	if [ $httpd -gt 1 ]
	    then
	    command="$httpdtest x 443 -c $i -l 20 -a -p /www/test.8192?$req -h $dir/httpd.tmp"
	else
	    command="$httpdtest $httpdip 443 -c $i -l 20 -a -p /www/test.8192?$req"
	fi
	name="tp-$req-c$i-n$j"
	benchmark $httpd $app $user $name "$command"
	j=$(( $j + 1))
      done
      i=$(( $i + 1))
    done
}

# left to right in figure from DStar paper
dotp 1 1 1 7 9 a2pdf
dotp 2 1 1 7 9 a2pdf
dotp 3 1 1 4 6 a2pdf
dotp 1 2 1 6 8 a2pdf
dotp 1 3 1 6 8 a2pdf
dotp 1 4 1 8 10 a2pdf
dotp 1 1 2 9 11 a2pdf
dotp 2 1 2 5 7 a2pdf
dotp 3 1 2 4 6 a2pdf
dotp 1 2 2 10 12 a2pdf
dotp 1 3 2 9 11 a2pdf
dotp 2 3 1 14 16 a2pdf
dotp 2 3 2 14 16 a2pdf

dotp 1 1 1 7 10 cat
dotp 2 1 1 7 9 cat
dotp 3 1 1 12 14 cat
dotp 1 2 1 12 14 cat
dotp 1 3 1 12 14 cat
dotp 1 4 1 10 12 cat
dotp 1 1 2 7 9 cat
dotp 2 1 2 12 14 cat
dotp 3 1 2 16 18 cat
dotp 1 2 2 11 13 cat
dotp 1 3 2 11 13 cat
dotp 2 3 1 10 12 cat
dotp 2 3 2 12 14 cat

#benchmark httpd app user name command
#benchmark 1 1 1 "tp-pdf" "../obj/test/httpd-test 171.66.3.200 443 -c 5 -l 20 -a -p /www/test.8192?a2pdf"
#benchmark 1 1 1 "tp-cat" "../obj/test/httpd-test 171.66.3.200 443 -c 5 -l 20 -a -p /www/test.8192?cat"

echo "All done!"
