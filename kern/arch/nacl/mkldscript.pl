#!/usr/bin/perl

open(F, "$ARGV[0] --verbose |") || die "$ARGV[0] bad linker?";
while ($line = <F>) {
    if ($line =~ m/\=\=\=\=\=\=/) {
	last;
    }
}

print "#include <machine/memlayout.h>\n";

while ($line = <F>) {
    if ($line =~ m/\=\=\=\=\=\=/) {
	last;
    } 
    $line =~ s/(__executable_start.*)(0x\d+)(.*)(0x\d+)(.*)/$1KBASE$3KBASE$5/g;
    print $line;
}
close(F);
