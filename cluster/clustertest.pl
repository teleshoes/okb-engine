#! /usr/bin/perl

# simple script for quick evaluation of the clustering stuff

# Exemple usage:  
# ( lbzip2 -d < clusters.csv.bz2 ; echo LEARN ; lbzip2 -d < grams-en-learn.csv.bz2 ; echo TEST ; lbzip2 -d < grams-en-test.csv.bz2 ) | ./clustertest.pl

use strict;
use Getopt::Std;
use File::Basename;

our ($opt_c, $opt_W, $opt_C, $opt_v, $opt_g);
getopts('c:C:W:vg');

my $clust_len = $opt_c;
my $limit_words_grams = $opt_W;
my $limit_clusters_grams = $opt_C;
my $verbose = $opt_v;
my $gram2 = $opt_g;

print "Settings: Cluster id length: " . ($clust_len || "unlimited") . ", word grams max: " . ($limit_words_grams || "unlimited") . ", cluster grams max: " . ($limit_clusters_grams || "unlimited") . "\n";

sub usage {
    print "usage: ( cat clusters ; echo \"LEARN\" ;  cat learn_ngrams ; echo \"TEST\" ; cat test_ngrams ) | " . basename($0) . " [<options>]\n";
    exit(1);
}

usage() if $ARGV[0];

# read clusters
my %WORD2C;
my %CLUST_COUNT;
while(<STDIN>) {
    chomp;
    next unless $_;
    last if $_ eq 'LEARN';

    my ($cluster, $word) = split(';', $_);
    die "bad line: $_" unless $word;
    $cluster = substr($cluster, 0, $clust_len) if $clust_len;
    $WORD2C{$word} = $cluster;
    $CLUST_COUNT{$cluster} = 0;
}
printf("Words: %d, Clusters: %d\n", scalar(keys %WORD2C), scalar(keys %CLUST_COUNT));

$WORD2C{'#TOTAL'} = '#TOTAL';
$WORD2C{'#NA'} = '#NA';
$WORD2C{'#START'} = '#START';

my $progress=(-t STDOUT)?1:undef;

# read learn n-grams
my %GRAM;
my %CGRAM;
my %WORD_COUNT;
my $n = 0;
my $total_learn = 0;
while(<STDIN>) {
    chomp;
    next unless $_;
    last if $_ eq 'TEST';

    my @tmp = split(';', $_);
    die "Bad line: $_" unless scalar(@tmp) == 4;

    my ($count, $w1, $w2, $w3) = @tmp;

    next if $w1 eq '#NA' && $w2 ne '#START';  # filter N-grams with N < 3
    $w1 = '#NA' if $gram2; # rebuild 2-grams based on 3-grams

    my ($c1, $c2, $c3) = ($WORD2C{$w1}, $WORD2C{$w2}, $WORD2C{$w3});

    next unless $c1 && $c2 && $c3;  # skip N-grams with unknown words

    if ($w3 ne '#TOTAL') {
	$WORD_COUNT{$w3} += $count;
	$CLUST_COUNT{$c3} += $count;
    }

    $GRAM{"$w1;$w2;$w3"} += $count;
    $CGRAM{"$c1;$c2;$c3"} += $count;
    $total_learn += $count;

    $n ++;
    print STDERR "." if $progress && ! ($n % 100000);
}
print STDERR "\n" if $progress;
printf("N-Grams: %d, Cluster N-Grams: %d\n", scalar(keys %GRAM), scalar(keys %CGRAM));

# truncate n-grams lists
if ($limit_words_grams) {
    my @gram = sort { $GRAM{$b} <=> $GRAM{$a}; } (keys %GRAM);
    foreach my $key (splice(@gram, $limit_words_grams)) {
	delete $GRAM{$key};
    }
}
if ($limit_clusters_grams) {
    my @gram = sort { $CGRAM{$b} <=> $CGRAM{$a}; } (keys %CGRAM);
    foreach my $key (splice(@gram, $limit_clusters_grams)) {
	delete $CGRAM{$key};
    }
}

# recompute denominators trigrams
foreach my $hash (\%CGRAM, \%GRAM) {
    foreach my $k (keys %$hash) {
	$hash->{$k} = 0 if substr($k,-7) eq ';#TOTAL';
    }
    foreach my $k (keys %$hash) {
	next if substr($k,-7) eq ';#TOTAL';
	my $d = $k;
	$d =~ s/;[^;]*$/;#TOTAL/;
	$hash->{$d} += $hash->{$k};
    }
}

my $gram_count = scalar(keys %GRAM);
my $cgram_count = scalar(keys %CGRAM);
print "N-Grams: ${gram_count}, Cluster N-Grams: ${cgram_count}\n";


# read test n-grams
my ($total_test, $p_log, $p_cluster_log, $hit, $hit_cluster, $c_pass, $w_pass);
while(<STDIN>) {
    chomp;
    next unless $_;

    my @tmp = split(';', $_);
    die "Bad line: $_" unless scalar(@tmp) == 4;

    my ($count, $w1, $w2, $w3) = @tmp;
    next if $w3 eq '#TOTAL'; # we are only using n-grams for word + context count, we can ignore #TOTAL lines

    next unless $WORD_COUNT{$w3}; # a word in cluster list has not been found in learning corpus -> ignore

    next if $w1 eq '#NA' && $w2 ne '#START';  # filter N-grams with N < 3
    $w1 = '#NA' if $gram2; # rebuild 2-grams based on 3-grams

    my ($c1, $c2, $c3) = ($WORD2C{$w1}, $WORD2C{$w2}, $WORD2C{$w3});

    next unless $c1 && $c2 && $c3;  # skip N-grams with unknown words
    
    my $wcount = $GRAM{"$w1;$w2;$w3"};
    my $wcount_d = $GRAM{"$w1;$w2;#TOTAL"};

    my $ccount = $CGRAM{"$c1;$c2;$c3"};
    my $ccount_d = $CGRAM{"$c1;$c2;#TOTAL"};

    $total_test += $count;

    $hit += $count if $wcount;
    $hit_cluster += $count if $ccount;

    $p_log += $count * log($wcount / $wcount_d) if $wcount;
    $p_cluster_log += $count * (log($ccount / $ccount_d) + log($WORD_COUNT{$w3} / $CLUST_COUNT{$c3})) if $ccount; 

    $w_pass += $count * $GRAM{"$w1;$w2;#TOTAL"}; # QQQ le bug c'est que je vois plusieurs fois le même 2-gram (???????)
    $c_pass += $count * $CGRAM{"$c1;$c2;#TOTAL"};

    printf("($w1;$w2;$w3) $wcount/$wcount_d -> $ccount/$ccount_d +++ %f -> %f\n", 
	   ($wcount?100.0 * $wcount / $wcount_d:-1), ($ccount?100.0 * $WORD_COUNT{$w3} / $CLUST_COUNT{$c3} * $ccount / $ccount_d:-1))  if $verbose;

    $n ++;
    print STDERR "." if $progress && ! ($n % 100000);
}
print STDERR "\n" if $progress;

printf("Test N-Grams: %d\n", $total_test);

my ($pw, $pc) = (exp(- $p_log / $total_test), exp(- $p_cluster_log / $total_test));
my ($hw, $hc) = (100.0 * $hit / $total_test, 100.0 * $hit_cluster / $total_test);
my ($aw, $ac) = (100.0 * $w_pass / $total_test / $total_test, 100.0 * $c_pass / $total_test / $total_test);

printf("Clusters: %d, Words[p=%.2f, hit=%.2f, %%pass=%.2f], Clusters[p=%.2f, hit=%.2f, %%pass=%.2f]\n",
       $cgram_count,
       $pw, $hw, $aw,
       $pc, $hc, $ac);

print;

printf("Result;%d;%d;%d;%.2f;%.2f;%.2f;%.2f;%.2f;%.2f\n",
       scalar(keys %CLUST_COUNT),
       $gram_count, $cgram_count,
       $pw, $pc, $hw, $hc, $aw, $ac);
