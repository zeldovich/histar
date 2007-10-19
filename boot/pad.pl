#!/bin/perl

@ARGV == 2 || die "usage: pad file size\n";
open(SIG, $ARGV[0]) || die "open $ARGV[0]: $!";

$fn = $ARGV[0];
$size = $ARGV[1];
$n = sysread(SIG, $buf, $size);

if($n > $size){
	print STDERR "$fn too large: $n bytes (max $size)\n";
	exit 1;
}

print STDERR "$fn is $n bytes (max $size)\n";

$buf .= "\0" x ($size-$n);

open(SIG, ">$ARGV[0]") || die "open >$ARGV[0]: $!";
print SIG $buf;
close SIG;
