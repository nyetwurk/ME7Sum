#!/usr/bin/perl

use strict;
use Data::Dumper;

my @ary;
my $len=0;

while (<>) {
    chomp;
    s/\/\///;
    s/#//;
    s/\s+//;
    my @a=(split(' ',$_));
    next unless scalar(@a);
    $len = scalar(@a) if (scalar(@a)>$len);
    push @ary, \@a;
}

my @needle = (0xff) x $len;
my @mask = (0) x $len;

#print Dumper(\@ary);

for (my $i=0; $i<$len; $i++) {
    my @col = map $_->[$i], @ary;
    for my $v (@col) {
	$needle[$i] &= hex($v);
	$mask[$i] |= hex($v);
    }
}

for (my $i=0; $i<$len; $i++) {
    printf("0x%02x,", $needle[$i]&0xff);
}
print "\n";

for (my $i=0; $i<$len; $i++) {
    printf("0x%02x,", ($needle[$i] | ~$mask[$i])&0xff);
}
print "\n";
