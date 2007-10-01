#!/bin/sh
# adm-file, httpd-file, app-file, and user-file contain ip addresses
# seperated by newlines.  The number of ip addreses must equal the
# number of newlines.  adm-file should contain IPMI ip addreses.

login=XXX
pass=XXX

ipmicli=./ipmicli.x86_64
restartfile=ipmi_restart.batch
sshkey=../keys/id_rsa
waittime=120

if [ $# -ne "4" ]
then
    echo "usage: $0 adm-file httpd-file app-file user-file"
    exit 1
fi

admlist=$1
httpdlist=$2
applist=$3
userlist=$4

appcount=$( wc -l $applist | grep -o "[0-9*] " )
usercount=$( wc -l $userlist | grep -o "[0-9*] " )

httpdmomopts="--http_auth_enable 1 --http_dj_enable 1"
httpdmomopts="$httpdmomopts --dj_app_server_count $appcount"
httpdmomopts="$httpdmomopts --dj_user_server_count $usercount"

function restart {
    cat $1 | while read ipaddr
      do
      echo "Restarting $ipaddr..."
      $ipmicli $ipaddr $login $pass $restartfile
    done
}

function bootstrap {
    httpdip=$1
    echo "Bootstrapping $httpdip..."

    ssh -n -i $sshkey -l root $httpdip "/bin/httpd_mom $httpdmomopts"

    i=0
    cat $applist | while read appip
      do
      ssh -n -i $sshkey -l root $httpdip "/bin/bootstrapc $appip app $i"
      i=$(( $i + 1 ))
    done

    i=0
    cat $userlist | while read userip
      do
      ssh -n -i $sshkey -l root $httpdip "/bin/bootstrapc $userip user $i"
      i=$(( $i + 1 ))
    done

    echo "Done with $httpdip!"
}

restart $admlist

echo "Waiting $waittime seconds for machines to restart..."
sleep $waittime

cat $httpdlist | while read ipaddr
  do
  bootstrap $ipaddr
done

exit
