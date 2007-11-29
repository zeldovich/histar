#!/bin/ksh
mkdir /usr
mkdir /lib
ln -s /bin/*.so /lib

if [ -f /bin/gcc.tar.gz ]; then
    echo "$0: unpacking gcc.."
    tar -C /usr -xzmf /bin/gcc.tar.gz
fi

if [ -f /bin/include.tar.gz ]; then
    echo "$0: unpacking headers.."
    tar -C /usr -xzmf /bin/include.tar.gz
fi

if [ -f /bin/fonts.tar.gz ]; then
    echo "$0: unpacking fonts.."
    tar -C / -xzmf /bin/fonts.tar.gz
fi

if [ -f /bin/fc-cache ]; then
    echo "$0: generating font cache.."
    mkdir -p /var/cache/fontconfig
    fc-cache
fi

test -f /bin/vim && ln -s vim /bin/vi

mkdir /sample
mkdir /sample/wrap
cat > /sample/wrap/hello.c <<EOM
#include <stdio.h>

int
main(int ac, char **av)
{
    printf("Hello world!\n");
}
EOM

cat > /sample/wrap/compile-and-run.sh <<EOM
#!/bin/sh
MACHINE=`uname -m`
test \$MACHINE == i386 && CFLAGS="\$CFLAGS -m32 -static"
gcc \$CFLAGS /sample/wrap/hello.c -o /tmp/hello
/tmp/hello
EOM

cat > /sample/wrap/clamscan.sh <<EOM
#!/bin/sh
exec /bin/clamscan -d /bin/clamav_main.cvd -d /bin/clamav_daily.cvd "\$@"
EOM

cat > /sample/wrap/malicious.sh <<EOM
#!/bin/sh
mkdir /public
cp /etc/passwd /public
cp /etc/ssh_host_rsa_key /public
echo Garbage > /etc/ssh_host_rsa_key
EOM

cat > /sample/wrap/README <<EOM
To compile and run the "hello.c" program in complete isolation, type:

    # wrap /bin/sh /sample/wrap/compile-and-run.sh

Note that the wrapped process receives its own private /tmp directory,
so the /tmp/hello binary generated by gcc is not visible in the global
/tmp directory.

Similarly, to run the ClamAV virus scanner on files in /etc in isolation, type:

    # wrap /bin/sh /sample/wrap/clamscan.sh /etc

To see how the wrap program prevents leaks, try running malicious.sh.
This script tries to copy the /etc/passwd file and the SSH host key into
some public directory (/public, which it tries to create) and then corrupt
them in place.  The wrap program prevents this from happening:

    # wrap /bin/sh /sample/wrap/malicious.sh

The wrap program also kills any runaway script that does not terminate
within a 15 second timeout, and ensures that no copies of the data linger
after the script is killed.

EOM

