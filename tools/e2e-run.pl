#! /usr/bin/perl
#
# end-to-end tests evaluation and optimization
# 

use strict;
use Data::Dumper;
use Clone qw(clone);
use List::Util qw(min max);

my @ALL_SCORES = ("s3", "c3", "s2", "c2", "s1");

sub load {
    my ($fh, $trunc) = @_;
    my @tests;
    my $current;

    my $c = 0;
    while(<$fh>) {
	chomp;
	
	$c ++;
	last if $trunc && $c > $trunc;

	if (! $_) {
	    push(@tests, $current) if $current;
	    $current = undef;
	} else {
	    my ($testid, $count, $expected, $actual, $score, $star, $class, $predict) = split(';', $_);
	    die "Bad line; $_" unless $score;
	    if (! $current) {
		$current->{'id'} = $testid;
		$current->{'count'} = $count;
	    }
	    $current->{'expected'} = $actual if $expected;

	    my $word = { 'score' => $score, 'star' => $star, 'class' => $class };

	    $predict = undef if $predict eq 'None';
	    foreach (split(' ', $predict)) {
		m/^([a-z0-9-]+)=(.*)$/ || die "Bad predict field: $_";
		$word->{$1} = $2;
	    }

	    foreach my $sc (@ALL_SCORES) {
		my $value = $word->{$sc};
		if ($value) {
		    $value =~ m/^stock=([\.e0-9]+)\/([\.e0-9]+)(~coef=([\.e0-9]+)){0,1}$/ || die "bad value: $sc = [$value]";
		    my ($num, $den, $coef) = ($1, $2, $4);
		    $word->{"num-$sc"} = $num;
		    $word->{"den-$sc"} = $den;
		    $word->{"coef-$sc"} = $coef;
		}
	    }

	    $current->{'candidates'}->{$actual} = $word;
	}

    }
    push(@tests, $current) if $current;
    print scalar(@tests) . " tests loaded\n";
    return \@tests;
}

sub hash2txt {
    my ($hash) = @_;
    my @tmp;
    foreach (sort(keys %$hash)) {
	my ($key, $value) = ($_, $hash->{$_});
	#next if $key =~ m/^_/;
	$value = sprintf("%.2e", $value) if $value =~ m/^[0-9\.\-e]+$/i && $value =~ m/\./;
	push(@tmp, "$key=$value")
    }
    return join(' ', @tmp) || "None";
}

sub score_log {
    my ($score) = @_;
    return 1 if $score >= 1;
    return 0 if $score <= 0;
    $score = (log($score) / log(10) + 8) / 8;
    return 0 if $score < 0;
    return $score;
}
    

sub score1 {
    my ($wi, $params, $max_score) = @_;

    my $score = 0;
    $score = $wi->{'score'} * ($max_score ** $params->{'max_pow'}) unless $params->{"ignore_curve"};

    my ($predict_score, $max_predict_score);
    foreach my $p (@ALL_SCORES) {
	my $proba = $wi->{"probas-$p"};
	my $score1 = ($params->{'log'}?score_log($proba):$proba) * $params->{$p};
	
	my $count = $wi->{"num-$p"};
	my $min_count = $params->{'min_count'};
	$score1 = 0 if $count && $min_count && $count < $min_count;  # prune low count n-grams

	$predict_score += $score1;
	$max_predict_score = max($max_predict_score, $score1);
	last if $score1 && $params->{'backoff'};
    }

    $predict_score = $max_predict_score if $params->{'max'};

    $predict_score = score_log($predict_score) if ! $params->{'log'};

    return $score + $predict_score;
}

sub filter {
    my ($test, $params) = @_;
    # set _predict_score for all candidates

    my $flt_max = $params -> {'flt_max'};
    my $flt_coef = $params -> {'flt_coef'};
    my $flt_min_count = $params -> {'flt_min_count'} || 25;
    die unless $flt_max && $flt_coef;

    my %score_count;
    my %score_coef;
    foreach my $word (keys %{$test->{'candidates'}}) {
	my $c = $test->{'candidates'}->{$word};
	foreach my $sc (@ALL_SCORES) {
	    $score_count{$sc} += $c ->{"num-$sc"};
	}
    }
    my $total_coef = 0;
    foreach my $sc (@ALL_SCORES) {
	my $num = $score_count{$sc};
	if ($num >= $flt_min_count) {
	    $score_coef{$sc} = 1;
	    $total_coef += 1;
	    last;
	}
	$score_coef{$sc} = $num / $flt_min_count;
	$total_coef += $score_coef{$sc};
    }
    $total_coef = 1 unless $total_coef;

    my $max_proba = 0;
    foreach my $word (keys %{$test->{'candidates'}}) {
	my $c = $test->{'candidates'}->{$word};
	my $proba = 0;
	foreach my $sc (@ALL_SCORES) {
	    $proba += $c->{"probas-$sc"} * $score_coef{$sc} / $total_coef;
	}
	$max_proba = $proba if $proba > $max_proba;
	$c->{'_flt_proba'} = $proba;
    }

    foreach my $word (keys %{$test->{'candidates'}}) {
	my $c = $test->{'candidates'}->{$word};

	my $proba = $c->{'_flt_proba'};
	my $score;
	if ($proba) {
	    my $x = log($max_proba / $proba) / log(10);
	    $score = $flt_max - $flt_coef *  ($x ** 4) / (3 + $x ** 3) / (0.2 + $x / 4);
	} else {
	    $score = 0;
	}
	$c->{'_predict_score'} = $score;
    }
    
}

sub eval1 {
    my ($test, $params) = @_;
    
    my $expected = $test->{'expected'};

    my $max_curve_score = 0;
    foreach my $word (keys %{$test->{'candidates'}}) {
	my $sc = $test->{'candidates'}->{$word} -> {'score'};
	$max_curve_score = $sc if $sc > $max_curve_score;
    }

    filter($test, $params) if $params -> {'flt_max'};

    my %scores;
    my ($max, $max_word);
    foreach my $word (keys %{$test->{'candidates'}}) {
	my $score_word = $test->{'candidates'}->{$word}->{'_predict_score'}; # filter

	$score_word = score1($test->{'candidates'}->{$word}, $params, $max_curve_score) unless defined $score_word; # default case

	$scores{$word} = $score_word; # aggregated score
	($max, $max_word) = ($score_word, $word) if $word ne $expected && $score_word > $max;
    }

    my ($ok, $score);
    my $score_exp = $scores{$expected};
    my $score1 = ($score_exp - $max);
    $score1 = $score1 / (1 + 10 * abs($score1));

    if ($score1 < 0) {
	$score = $score1;
	$ok = 0;
    } else {
	$score = 1 + $score1;
	$ok = 1;
    }

    if ($params->{'verbose'} || ($params->{'verbose_nok'} && ! $ok)) {
	print "--- Test: " . $test->{'id'} . " count: " . $test->{'count'} . " Params: " . hash2txt($params) . "\n";
	
	foreach my $word (sort { $scores{$b} <=> $scores{$a}; } keys %scores) {
	    my $score_word = $scores{$word};
	    print(($word eq $expected?"[*]":"   ") . " score[$word] = $score_word -> " . hash2txt($test->{'candidates'}->{$word}) . "\n");
	}

	printf("Result: score=%.3f ok=%d\n\n", $score, $ok?1:0);
	print;
    }

    return ($score, $ok);
}

sub test_eval {
    my ($tests, $params, $silent) = @_;
    my ($total, $okcount, $total_count);

    # evaluate test corpus    
    foreach my $test (@$tests) {
	my($score, $ok) = eval1($test, $params);
	my $count = $test -> {'count'}; # Oops, i forgot to add a coefficient for n-gram count :-) LOL !
	$total += $count * $score;
	$total_count += $count;
	$okcount += $count if $ok;
    }

    $total /= $total_count;

    printf("=== Eval [%s] --> Score=%.3f - OK=%d (%d%%)\n",
	   hash2txt($params),
	   $total, $okcount, 100 * $okcount / $total_count) unless $silent;

    return ($total, $okcount);
}

sub optim { # crude gradient descent
    my ($tests, $params) = @_;
    my @optim = split(',', $params->{'optim'});

    my $delta = $params->{'delta'} || 0.02;
    my $coef = $params->{'coef'} || 20;

    my $last_result;
    while(1) {
	my ($result, $okcount) = test_eval($tests, $params);

	last if $result < $last_result * 1.00001;

	my $new_params = clone($params);
	foreach my $pid (@optim) {
	    my $params_try = clone($params);
	    $params_try->{$pid} = $params->{$pid} - $delta;
	    my ($result1, $ok1) = test_eval($tests, $params_try, 1);
	    $params_try->{$pid} = $params->{$pid} + $delta;
	    my ($result2, $ok2) = test_eval($tests, $params_try, 1);

	    my $new = $params->{$pid} + $coef * ($result2 - $result1) / $delta;
	    $new = 0.01 if $new < 0.01;

	    $new_params->{$pid} = $new;
	    printf("   Param[$pid] = %.5e --> %.5e\n", $params->{$pid}, $new_params->{$pid});

	}
	$params = $new_params;

    }

}

# -- main
my %params;
foreach (@ARGV) {
    die "Bad arg: $_" unless $_ =~ m/^([a-z0-9_]+)=(.*)$/;
    $params{$1} = $2;
}
my $tests = load(*STDIN, $params{'trunc'});
print Dumper($tests) if $params{'dump'};
if ($params{"optim"}) {
    optim($tests, \%params);
} else {
    test_eval($tests, \%params);
}
