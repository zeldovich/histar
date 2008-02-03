#!/usr/bin/perl -w
# Expects the number of connections completed to be on the second 
# to last line of the .out files from dj-bench.pl

use strict;

my $ac = @ARGV;

($ac >= 3) || die "usage: name clientmin clientmax [ numruns ] [ duration ]\n";

my $name = $ARGV[0];
my $clientmin = $ARGV[1];
my $clientmax = $ARGV[2];
my $numruns = 3;
my $duration = 20;
if ($ac >= 4) {
    $numruns = $ARGV[3];
}
if ($ac >= 5) {
    $duration = $ARGV[4];
}

my $best_ave = 0;
my $best_clients = 0;
for (my $j = $clientmin; $j <= $clientmax; $j++) {
    my @conns;
    for (my $i = 0; $i < $numruns; $i++) {
	my $fname = "$name-c$j-n$i.out";
	open(F, $fname) || die "unable to open $fname";
	
	my $i = 0;
	my @lines = <F>;
	my $line;
	while ($line = pop @lines) {
	    chomp $line;
	    if ($i == 1) {
		push(@conns, $line);
	    }
	    if ($line =~ m/.*error.*/i) {
		print "WARNING ($fname): $line\n";
	    }
	    $i++;
	}
	close(F);
    }

    my $total = 0;
    foreach my $count (@conns) {
	$total += $count;
    } 
    my $ave = $total / $numruns;
    
    if ($ave > $best_ave) {
	$best_ave = $ave;
	$best_clients = $j;
    }
}

my $persec = $best_ave / $duration;

$persec = sprintf("%.2f", $persec);
$best_ave = sprintf("%.2f", $best_ave);

print "$name\t$best_clients\t$best_ave\t$persec\n";
