#! /usr/bin/perl
#
# end-to-end tests evaluation and optimization
# 

use strict;
use Data::Dumper;
use Clone qw(clone);
use List::Util qw(min max);

sub load {
    my ($fh) = @_;
    my @tests;
    my $current;

    while(<$fh>) {
	chomp;
	
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
		m/^([a-z0-9]+)=(.*)$/ || die "Bad predict field: $_";
		$word->{$1} = $2;
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
	next if $key =~ m/^_/;
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

    my $score = $wi->{'score'} * ($max_score ** $params->{'max_pow'});

    foreach my $p ("s3", "c3", "s2", "c2", "s1") {
	my $score1 = score_log($wi->{$p});
	$score += $score1 * $params->{$p};
	last if $score1 && $params->{'backoff'};
    }

    return $score;
}

sub eval1 {
    my ($test, $params) = @_;
    
    my $expected = $test->{'expected'};
    my $ok = 1;
    my $score = 0;

    my $max_score = 0;
    foreach my $word (keys %{$test->{'candidates'}}) {
	my $sc = $test->{'candidates'}->{$word} -> {'score'};
	$max_score = $sc if $sc > $max_score;
    }

    my %scores;
    foreach my $word (keys %{$test->{'candidates'}}) {
	my $score_word = score1($test->{'candidates'}->{$word}, $params, $max_score);
	$scores{$word} = $score_word;
    }

    my $score_exp = $scores{$expected};
    my $score = 1;
    foreach my $word (keys %{$test->{'candidates'}}) {
	next if $word eq $expected;	
	my $score_word = $scores{$word};
	$ok = undef if $score_word > $score_exp;

	my $score1 = ($score_exp - $score_word);
	$score1 = $score1 / (1 + 100 * abs($score1));
	$score = 1 + $score1 if $score1 > 0 && $score > 0 && 1 + $score1 > $score;
	$score = $score1 if $score1 < $score && $score1 <= 0;
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
    my ($total, $okcount);

    # evaluate test corpus
    foreach my $test (@$tests) {
	my($score, $ok) = eval1($test, $params);
	$total += $score;
	$okcount ++ if $ok;
    }

    $total /= scalar(@$tests);

    printf("=== Eval [%s] --> Score=%.3f - OK=%d (%d%%)\n",
	   hash2txt($params),
	   $total, $okcount, 100 * $okcount / scalar(@$tests)) unless $silent;

    return ($total, $okcount);
}

sub optim { # crude gradient descent
    my ($tests, $params) = @_;
    my @optim = split(',', $params->{'optim'});

    my $delta = $params->{'delta'} || 0.01;

    my $done = undef;
    while(! $done) {
	$done = 1;
	my ($result, $okcount) = test_eval($tests, $params);

	foreach my $pid (@optim) {
	    print("Optim $pid ...\n");
	    my $params_try = clone($params);
	    $params_try->{$pid} = $params->{$pid} - $delta;
	    my ($result1, $ok1) = test_eval($tests, $params_try, 1);
	    $params_try->{$pid} = $params->{$pid} + $delta;
	    my ($result2, $ok2) = test_eval($tests, $params_try, 1);

	    next if $result1 < $result * 1.00001 && $result2 < $result * 1.00001;

	    $done = undef;

	    my $p0 = $params->{$pid};
	    my $sign = ($result2 - $result1) <=> 0;
	    $params->{$pid} = max(0, $p0 + $sign * $delta);
	    printf("   Param[$pid] = %.3e\n", $params->{$pid});

	    my $n = 1;
	    my $last_result = ($result2 > $result1)?$result2:$result1;

	    while (1) {
		$params_try->{$pid} = max(0, $p0 + $sign * $delta * (2 ** $n));
		my ($result0, $ok0) = test_eval($tests, $params_try, 1);
		last if $result0 <= $last_result;
		$params->{$pid} = $params_try->{$pid};
		$last_result = $result0;
		printf("   Param[$pid] = %.3e --> score = %.3f - OK = %d\n", $params_try->{$pid}, $result0, $ok0);
		$n ++;
	    }

	}

    }

}

# -- main
my %params;
foreach (@ARGV) {
    die "Bad arg: $_" unless $_ =~ m/^([a-z0-9_]+)=(.*)$/;
    $params{$1} = $2;
}
my $tests = load(*STDIN);
print Dumper($tests) if $params{'dump'};
if ($params{"optim"}) {
    optim($tests, \%params);
} else {
    test_eval($tests, \%params);
}
