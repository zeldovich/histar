#!/bin/perl

@ARGV == 2 || die "usage: pad file size\n";
open(SIG, $ARGV[0]) || die "open $ARGV[0]: $!";

$fn = $ARGV[0];
$maxsize = $ARGV[1];

$size = (stat($fn))[7];
if ($size > $maxsize) {
	print STDERR "$fn too large: $size bytes (max $maxsize)\n";
	exit 1;
}
print STDERR "$fn is $size bytes (max $maxsize)\n";

$n = sysread(SIG, $buf, $maxsize);
$n == $size || die "stat wierdness $n != $size\n";

$buf .= "\0" x ($maxsize-$n);

open(SIG, ">$ARGV[0]") || die "open >$ARGV[0]: $!";
print SIG $buf;
close SIG;
