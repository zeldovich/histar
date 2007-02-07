#!/bin/perl -w 

## moscow ip
$host =        "http://171.66.3.151";
$path_prefix = "/~silasb/";
$dst_prefix = "/bin/";

(@ARGV > 0) || die("file name missing\n");

$fn = $ARGV[0];

$arg0 = $host . $path_prefix . $fn;
$arg1 = $dst_prefix . $fn;

system("/bin/fetch " . $arg0 . " " . $arg1);
