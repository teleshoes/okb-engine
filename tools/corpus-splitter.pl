#! /usr/bin/perl
# Corpus splitter
# e.g. for getting separate learning and test corpora

use strict;
use File::Basename;

my ($n1, $n2, $out1, $out2) = @ARGV;

if (! $out2 || $out2 !~ m/\.bz2$/ || $out1 !~ /\.bz2$/) {
    die "usage: " . basename($0) . " <line count 1> <line count 2> <out file 1.bz1> <out file 2.bz2>\n Input corpus fed from stding (uncompressed)";
}

open(O1, "| lbzip2 -9 > $out1") || die "open > $out1";
open(O2, "| lbzip2 -9 > $out2") || die "open > $out2";

my $c;
my $n = $n1 + $n2;
while(<STDIN>) {
    $c ++;
    if ($c % $n < $n1) {
	print O1 $_;
    } else {
	print O2 $_;
    }
}

close(O1);
close(O2);


