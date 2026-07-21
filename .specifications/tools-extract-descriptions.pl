#!/usr/bin/perl
# Regenerate the inventory section of APPS_FILE_INVENTORY.md from the file
# headers themselves, so the document cannot drift from the code.
#   cd apps && perl ../.specifications/tools-extract-descriptions.pl
use strict; use warnings; use File::Find;
my @f; find(sub{ return unless /\.(c|h|S)$/; my $p=$File::Find::name; $p=~s{^\./}{}; push @f,$p },".");
my %by;
for my $p (sort @f){
  open my $fh,"<",$p or next; local $/; my $t=<$fh>; close $fh;
  next unless $t =~ m{\A/\*{10,}\r?\n(.*?)\*{10,}/}s;
  my $h=$1; my @d; my $in=0;
  for my $l (split /\n/,$h){ $l=~s/\r$//; $l=~s{^\s*\*\s?}{}; $l=~s/\s+$//;
    if($l =~ /^GNU General Public License/){ $in=1; next }
    next unless $in; next if $l eq ""; push @d,$l; }
  my $d=join(" ",@d); next unless $d;
  my $dir=$p; $dir=~s{/[^/]+$}{}; $dir="(root)" if $dir eq $p;
  push @{$by{$dir}}, [$p,$d];
}
for my $dir (sort keys %by){
  printf("\n### %s  (%d files)\n", $dir, scalar @{$by{$dir}});
  for my $e (@{$by{$dir}}){ my ($p,$d)=@$e; $p=~s{.*/}{}; printf("  %-26s %s\n",$p,$d) }
}
