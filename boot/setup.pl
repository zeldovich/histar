#!/bin/perl

open(SIG, $ARGV[0]) || die "open $ARGV[0]: $!";

$n = sysread(SIG, $buf, 1000);

if($n > 512){
	print STDERR "boot block too large: $n bytes (max 512)\n";
	exit 1;
}

print STDERR "boot block is $n bytes (max 512)\n";

$buf .= "\0" x (512-$n);

open(SIG, ">$ARGV[0]") || die "open >$ARGV[0]: $!";
print SIG $buf;
close SIG;
