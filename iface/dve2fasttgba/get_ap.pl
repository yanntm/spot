#!/usr/bin/perl

#print STDERR "$ARGV[0]\n";
open IN, "< $ARGV[0]";
while (my $line = <IN>)
{
   if ($line =~ /^\s*process/)
   {
       chomp $line;
       my @proc = split(/ /, $line);
       my $pname = @proc[1];
       $pname =~ s/{//;
       $line = <IN> or die "Unexpected end of file";
       while ($line !~ /state/)
       {
           $line = <IN> or die "Unexpected end of file";
       }
       chomp $line;
       $line =~ s/state//;
       $line =~ s/\s//g;
       my @states = split(/,/, $line);
       for (my $i = 1 ; $i < scalar @states ; $i++)
       {
           my $state = @states[$i];
           $state =~ s/,//;
           $state =~ s/;//;
           print "$pname.$state\n";
       }
   }
}
close IN;
