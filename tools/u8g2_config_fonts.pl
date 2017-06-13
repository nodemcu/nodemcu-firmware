#!/usr/bin/perl -w

my @font_selection;

# scan provided header file for CONFIG_U8G2_FONT_SELECTION entry
while (<STDIN>) {
    if (/^\s*#\s*define\s+CONFIG_U8G2_FONT_SELECTION\s+"([^"]+)"/) {
        @font_selection = split(/,/, $1);
        last;
    }
}

print << 'HEADER';

#ifndef _U8G2_FONTS_H
#define _U8G2_FONTS_H

#define U8G2_FONT_TABLE_ENTRY(font)

#define U8G2_FONT_TABLE \
HEADER

foreach my $font (@font_selection) {
    print("  U8G2_FONT_TABLE_ENTRY($font) \\\n");
}

print << 'FOOTER';


#endif /* _U8G2_FONTS_H */
FOOTER
