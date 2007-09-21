#!/usr/bin/perl

my $bin = shift @ARGV or die "usage: $0 binfile";

open(F, "nm $bin |");
while (<F>) {
    next unless /^([0123456789abcdef]{16}) . (\S+)$/;
    my $addr = hex $1;
    $syms{$addr} = $2;
    #print "$addr: $2\n";
}
close(F);

print STDERR "reading profiling samples on stdin\n";

while (<>) {
    while (s/\s*([0123456789abcdef]+)//) {
	my $addr = hex $1;
	my $sym = find_symbol($addr);
	print "$sym\n";
    }
}

sub find_symbol {
    my ($addr) = @_;
    my $maxaddr = 0;
    foreach my $saddr (keys %syms) {
	$maxaddr = $saddr if $saddr <= $addr && $saddr > $maxaddr;
    }
    return $syms{$maxaddr};
}
