#!/usr/bin/perl -w

my (@displays, @font_selection);

# scan provided header file for CONFIG_UCG_DISPLAY and CONFIG_UCG_FONT_SELECTION entries
while (<STDIN>) {
    if (/^\s*#\s*define\s+CONFIG_UCG_DISPLAY_(\S+)\s+1/) {
        push(@displays, lc($1));
    }
    if (/^\s*#\s*define\s+CONFIG_UCG_FONT_SELECTION\s+"([^"]+)"/) {
        @font_selection = split(/,/, $1);
    }
}


print << 'HEADER';

#ifndef _UCG_CONFIG_H
#define _UCG_CONFIG_H

HEADER


print << 'DISPLAYS';

#define UCG_DISPLAY_TABLE_ENTRY(binding, device, extension)

DISPLAYS

print("#define UCG_DISPLAY_TABLE \\\n");
foreach my $display (@displays) {
    $display =~ /(\S+_\d\d)x/;
    my $extension = $1;
    print("  UCG_DISPLAY_TABLE_ENTRY(${display}_hw_spi, ucg_dev_${display}, ucg_ext_${extension}) \\\n");
}
print("\n");


print << 'FONTS';

#define UCG_FONT_TABLE_ENTRY(font)

#define UCG_FONT_TABLE \
FONTS

foreach my $font (@font_selection) {
    print("  UCG_FONT_TABLE_ENTRY($font) \\\n");
}


print << 'FOOTER';

#endif /* _UCG_CONFIG_H */
FOOTER
