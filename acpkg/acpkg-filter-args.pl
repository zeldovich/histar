#!/usr/bin/perl
my @nargv = ();
my $cmd = shift @ARGV;

foreach my $arg (@ARGV) {
    next if $arg =~ /^-[IL]\/usr(\/[\w\d]+)?\/(lib|include)(64)?$/;
    next if $arg =~ /^-[IL]\/usr\/(lib|include)(64)?/;
    push @nargv, $arg;
}

exec $cmd, @nargv;
