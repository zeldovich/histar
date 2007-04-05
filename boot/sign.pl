#!/bin/perl

open(SIG, $ARGV[0]) || die "open $ARGV[0]: $!";

$n = sysread(SIG, $buf, 1000);

if($n > 497){
	print STDERR "boot block too large: $n bytes (max 497)\n";
	exit 1;
}

print STDERR "boot block is $n bytes (max 510)\n";

$buf .= "\0" x (497-$n);

open(BS, $ARGV[1]) || die "open $ARGV[1]: $!";

$n = sysread(BS, $bsdata, 512);
if($n != 15){
	print STDERR "bsdata incorrect size: $n != 15)\n";
	exit 1;
}

$buf .= $bsdata;

open(SIG, ">$ARGV[0]") || die "open >$ARGV[0]: $!";
print SIG $buf;
close SIG;
