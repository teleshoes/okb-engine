#! /usr/bin/perl
# Allow to measure performance of a prediction algorithm (with a perplexity-like measure)
# Optionaly parameters auto-tuning (e.g. for linear interpolation or backoff)

use strict;
use File::Basename;
use Getopt::Std;

our($opt_o);
getopts('o');
my $optim = $opt_o;

my $learn_csv = shift(@ARGV);
my $test_corpus = shift(@ARGV);
my @params = @ARGV;

die "usage: " . basename($0) . " [-o] <predict db .csv.bz2> <cleaned test corpus .txt.bz2> [<param1> ... [<paramN>] ]" unless $test_corpus;

sub read_ngrams {
    my ($file) = @_;
    die "csv files must be .bz2 files" unless $file =~ m/\.bz2$/;

    print "Loading predict ngrams: $file ...\n";

    open(F, "lbzip2 -dc $file |") || die "< $file";
    my %lines;
    while(<F>) {
	chomp;
	my ($count, $w3, $w2, $w1) = split(';', $_);
	die unless $w1;
	$lines{"$w3;$w2;$w1"} = [ $count, $w3, $w2, $w1 ];
    }
    close F;

    my %grams;
    foreach my $key (keys %lines) {
	my ($count, $w3, $w2, $w1) = @{$lines{$key}};
	$key =~ s/^(\#NA;)*//;
	my $den = $lines{"$w3;$w2;#TOTAL"}->[0];
	if ($w1 eq '#TOTAL') {
	    $key =~ s/[^;]+$/#NOTFOUND/;
	    $grams{$key} = 1.0 / $den;  # smoothing (Laplace-like) -> perplexity can't be evaluated without smoothing
	} else {
	    my $den = $lines{"$w3;$w2;#TOTAL"}->[0];
	    die "No denominator line found?" unless $den;
	    $grams{$key} = 1.0 * ($count + 1) / $den;  # probability
	}
    }

    print "OK - " . scalar(keys %grams) . " N-grams\n";

    return \%grams;
}

sub read_corpus {
    my ($file) = @_;
    die "cleaned corpus files must be .bz2 files" unless $file =~ m/\.bz2$/;

    print "Loading cleaned test corpus: $file ...\n";

    open(C, "lbzip2 -dc $file |") || die "< $file";

    my %grams;
    while(<C>) {
	chomp;
	my @words = split(/\s+/, $_);
	if ($words[0] eq '#ERR') {
	    shift(@words);
	} else {
	    unshift(@words, '#START');
	}
	my @window;
	foreach my $word (@words) {
	    push(@window, $word);
	    shift(@window) if scalar(@window) > 3;
	    next if scalar(@window) == 1 && $window[0] eq '#START';
	    my $key = join(';', @window);
	    $grams{$key} ++;
	}
    }
    close C;

    print "OK - " . scalar(keys %grams) . " words\n";

    return \%grams;
}

sub get_gram_proba {
    my ($grams, $gram, $smoothing) = @_;
    return undef unless $gram;

    my $proba = $grams->{$gram};
    return $proba if $proba;

    return 0 unless $smoothing;

    # use smoothing
    my $gram2 = $gram;
    $gram2 =~ s/[^;]+$/#NOTFOUND/;
    $proba = $grams->{$gram2};
    return $proba || 0;
}

sub eval_interpolate {
    my($gram1, $gram2, $gram3, $grams, $params) = @_;
    my($lambda2, $lambda1) = @$params;
    
    my $p3 = get_gram_proba($grams, $gram3, 1);
    my $p2 = get_gram_proba($grams, $gram2, 1);
    my $p1 = get_gram_proba($grams, $gram1, 1);
    
    if ($gram3) {
	return $p3 * (1 - $lambda1 - $lambda2) + $lambda2 * $p2 + $lambda1 * $p1;
    } 
    
    if ($gram2) { # normalize 2-words context to [0,1]
	return ($p2 * $lambda2 + $p1 * $lambda1) / ($lambda2 + $lambda1);
    }
    
    return $p1;
}

# @@@@ oops, i just realized that perplexity is useless with my "backoff-with-fake-smoothing" ... :-)
#sub eval_backoff {
#    
#
#    my($gram1, $gram2, $gram3, $grams, $params) = @_;
#    my($alpha1, $alpha2) = @$params;
#    
#    my $p3 = get_gram_proba($grams, $gram3);
#    my $p2 = get_gram_proba($grams, $gram2);
#    my $p1 = get_gram_proba($grams, $gram1, 1);
#    
#    if ($gram3) {
#    	if ($p3) { return $p3; }
#	if ($p2) { return $alpha1 * $p2; }
#    	return $alpha2 * $p1;
#    }
#    
#    if ($gram2) {
#	if ($p2) { return $p2; }
#	my $alpha = ($alpha1 < $alpha2)?($alpha1 / $alpha2):1;
#	return $alpha * $p1;
#    } 
#    
#    return $p1;
#}

sub evaluate {
    my ($grams, $test, $eval_func, $params) = @_;

    my $total = 0;
    my $total_count = 0;

    foreach my $test_gram (keys %$test) {
	my $count = $test -> {$test_gram} + 1;
	my @gramlst = ($test_gram);
	unshift(@gramlst, $test_gram) while $test_gram =~ s/^[^;]+;//;
	push(@gramlst, undef) while scalar(@gramlst) < 3;
	my $proba = &$eval_func(@gramlst, $grams, $params);

	$total += log($proba) * $count;
	$total_count += $count;
    }

    my $result = $total / $total_count;
    return $result;
}

my $grams = read_ngrams($learn_csv);
my $test = read_corpus($test_corpus);

my $eval = \&eval_interpolate;

if ($optim) {
    # crude gradient descent

    # please tune this by hand if values are not good
    my $DELTA = 0.001;
    my $COEF = 0.1;

    while(1) { # you'll have to Ctrl-C when happy with the result
	my $result = evaluate($grams, $test, $eval, \@params);
	print "Perplexity(log): $result p=[". join(",", @params) . "]\n";

	my @new_params = @params;
	for(my $i = 0; $i < scalar(@params); $i ++) {
	    my @p = @params;
	    $p[$i] = $params[$i] - $DELTA;
	    my $result1 = evaluate($grams, $test, $eval, \@p);
	    $p[$i] = $params[$i] + $DELTA;
	    my $result2 = evaluate($grams, $test, $eval, \@p);

	    $new_params[$i] = $params[$i] + $COEF * ($result2 - $result1) / $DELTA / 2;
	}
	@params = @new_params;
    }
}

my $result = evaluate($grams, $test, $eval, \@params);
print "Perplexity(log): $result\n";

