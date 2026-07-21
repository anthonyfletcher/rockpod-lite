#!/usr/bin/perl
# Rewrite apps/ file headers into the RockPod-Lite format.
#
#   /***************************************************************************
#    * RockPod-Lite
#    *
#    * Original code from RockBox          <- only if Rockbox-derived
#    * was: apps/gui/icon.h                <- only if the file moved
#    * Copyright (c) 2005 by Kevin Ferrare <- every attribution line, verbatim
#    * GNU General Public License (version 2+)
#    *
#    * <description>
#    ****************************************************************************/
#
# Everything in the old banner that is not ASCII art, $Id$, or GPL boilerplate
# is treated as attribution and carried over verbatim. Nothing is invented.
use strict; use warnings;

my ($descmap, @files) = @ARGV;
die "usage: reheader.pl <descmap> <file>...\n" unless $descmap && @files;

my %desc;
open my $dm, "<", $descmap or die "$descmap: $!";
while (<$dm>) { chomp; next unless /\|/; my ($f,$d) = split /\|/, $_, 2; $desc{$f} = $d }
close $dm;

my $BAR = "/***************************************************************************";
my $END = " ****************************************************************************/";

sub wrap {
    my ($text, $width) = @_;
    my @out; my $cur = "";
    for my $w (split /\s+/, $text) {
        if (length("$cur $w") > $width) { push @out, $cur; $cur = $w }
        else { $cur = $cur ? "$cur $w" : $w }
    }
    push @out, $cur if $cur ne "";
    return @out;
}

my ($done, $skipped, @lost) = (0, 0);
for my $f (@files) {
    open my $fh, "<", $f or do { warn "  MISS $f\n"; next };
    local $/; my $t = <$fh>; close $fh;

    # Idempotency: never re-process a header we already wrote, or the new
    # header gets consumed as if it were an old banner and duplicated.
    if ($t =~ m{\A/\*{10,}\r?\n\s*\*\s*RockPod-Lite\b}) { $skipped++; next }

    my $was;
    $was = $1 if $t =~ s{\A/\* was: ([^\n*]+?) \*/\r?\n}{};

    # existing inserted description (from the earlier pass), if any
    my $predesc;
    if ($t =~ s{\A/\* ((?:[^*]|\*(?!/))*?) \*/\r?\n}{}s) {
        $predesc = $1; $predesc =~ s/\s*\n\s*\*\s*/ /g; $predesc =~ s/\s+/ /g;
        $predesc =~ s/^\s+|\s+$//g;
    }

    # Vendored third-party code: never touch. These carry their own upstream
    # licences (giflib is MIT/X11, tlsf is dual GPL/LGPL, the jpeg81/idct
    # files have their own attribution). Restamping them with "Original code
    # from RockBox / GPL v2+" would be false and a licence misstatement.
    # Exclusion is by LICENCE, not by path: a file in the vendored area that
    # carries a Rockbox banner was adapted by Rockbox under the GPL with the
    # original author's copyright preserved (tinflate.c, crc32.c), and is safe
    # to convert. One with no Rockbox banner is upstream's own file.
    if ($f =~ m{^viewers/image_viewer/(decoders/|tlsf\.)} && $t !~ /Jukebox/) {
        $skipped++;
        $t = ($predesc ? "/* $predesc */\n" : "") . $t;
        $t = "/* was: $was */\n" . $t if $was;
        open my $w, ">", $f; print $w $t; close $w; next;
    }

    # the old banner; absent for fork-original files, which still get a header
    my $banner = "";
    $banner = $1 if $t =~ s{\A(/\*{10,}.*?\*{10,}/)\r?\n}{}s;

    my $rockbox = ($banner =~ /Jukebox/) ? 1 : 0;

    my @attrib;
    for my $line (split /\n/, $banner) {
        $line =~ s/\r$//;
        next if $line =~ m{^/\*+$} or $line =~ m{^\s*\*+/$};
        $line =~ s{^\s*\*\s?}{};                    # strip leading " * "
        # keep interior blank lines: they group the original attribution
        if ($line =~ /^\s*$/) { push @attrib, ""; next }
        next if $line =~ /^\s*\$Id\$/;
        next if $line =~ m{^\s*(Open|Source|Jukebox|Firmware)\b};  # art labels
        next if $line =~ m{^[\s\\/_|.()'-]*$};                     # pure art
        next if $line =~ m{[\\/]_{2,}|\\_\_|_{5,}|\\/ *\\/};       # art fragments
        next if $line =~ /This program is free software/;
        next if $line =~ /modify it under the terms/;
        next if $line =~ /as published by the Free Software Foundation/;
        next if $line =~ /of the License, or \(at your option\)/;
        next if $line =~ /This software is distributed on an "AS IS"/;
        next if $line =~ /KIND, either express or implied/;
        push @attrib, $line;
    }

    # trim leading/trailing blanks and collapse runs
    shift @attrib while @attrib && $attrib[0]  eq "";
    pop   @attrib while @attrib && $attrib[-1] eq "";
    my @tidy; my $prev_blank = 0;
    for my $a (@attrib) {
        next if $a eq "" && $prev_blank;
        push @tidy, $a; $prev_blank = ($a eq "");
    }
    @attrib = @tidy;

    my $d = $desc{$f} // $predesc // "";
    unless ($d) { warn "  NO DESCRIPTION: $f\n" }

    my @h = ($BAR, " * RockPod-Lite", " *");
    push @h, " * Original code from RockBox" if $rockbox;
    push @h, " * was: $was" if $was;
    push @h, ($_ eq "" ? " *" : " * $_") for @attrib;
    push @h, " * GNU General Public License (version 2+)";
    if ($d) { push @h, " *"; push @h, " * $_" for wrap($d, 72) }
    push @h, $END;

    open my $w, ">", $f or die $!;
    print $w join("\n", @h), "\n", $t;
    close $w;
    $done++;
}
print "  rewritten: $done   no-banner (left alone): $skipped\n";
