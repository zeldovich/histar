#!/bin/sh
# host-file contains ip address of usable hosts.  adm-file contains
# IPMI ip addresses for all the hosts in host-file.  results and
# temporary files are placed in results-dir.

setup=./dj-httpd-setup.sh

if [ $# -ne "3" ]
then
    echo "usage: $0 host-file adm-file results-dir"
    exit 1
fi

hostlist=$1
admlist=$2
dir=$3

function fillfile {
    file=$1
    offset=$2
    count=$3

    i=0;
    while [ $i -lt $count ]
      do
      line=$(( $i + $offset ))
      ip=$( sed -n "$line,${line}p" $hostlist )
      echo "$ip" >> $dir/$file
      i=$(( $i + 1))
    done
}

function benchmark {
    httpd=$1
    app=$2
    user=$3
    name=$4
    benchcmd=$5
    echo "Benchmarking $httpd httpd $app app $user user..."
    
    rm -f $dir/httpd.tmp
    rm -f $dir/app.tmp
    rm -f $dir/user.tmp
    fillfile "httpd.tmp" 1 $httpd
    fillfile "app.tmp" $(( 1 + $httpd )) $app
    fillfile "user.tmp" $(( 1 + $httpd + $app)) $user

    out=$dir/$httpd-$app-$user-$name
    rm -f $out
    echo "Running setup script..."
    $setup $admlist $dir/httpd.tmp $dir/app.tmp $dir/user.tmp > $out

    echo "Running: $benchcmd"
    echo "--------------" >> $out
    echo "|BENCH OUTPUT|" >> $out
    echo "--------------" >> $out
    time=`(time $benchcmd >> $out) 2>&1`
    echo $time >> $out
}

#benchmark 1 1 1 "ls" "ls -l"
#benchmark httpd app user name command

echo "All done!"
