#!/bin/perl

open(D, "+<".$ARGV[0]) || die "open $ARGV[0]: $!";
@stat = stat(D);
$sectors = $stat[7] / 512;

$buf  = "\x00" x 446;
$buf .= "\x00\x00\x00\x00";
$buf .= "\xBC\x00\x00\x00";
$buf .= pack("L", 1);
$buf .= pack("L", $sectors - 1);
$buf .= "\x00" x 48;
$buf .= "\x55\xAA";

syswrite D, $buf, 512;
close D;

