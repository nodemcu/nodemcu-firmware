#!/usr/bin/perl -w

my (@i2c_displays, @spi_displays);

# scan provided header file for CONFIG_U8G2_I2C and CONFIG_U8G2_SPI entries
while (<STDIN>) {
    if (/^\s*#\s*define\s+CONFIG_U8G2_I2C_([^_]+)_(\S+)\s+1/) {
        push(@i2c_displays, lc($1)."_i2c_".lc($2));
    }
    if (/^\s*#\s*define\s+CONFIG_U8G2_SPI_(\S+)\s+1/) {
        push(@spi_displays, lc($1));
    }
}


print << 'HEADER';

#ifndef _U8G2_DISPLAYS_H
#define _U8G2_DISPLAYS_H

#define U8G2_DISPLAY_TABLE_ENTRY(function, binding)

HEADER

print("#define U8G2_DISPLAY_TABLE_I2C \\\n");
foreach my $display (@i2c_displays) {
    print("  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_${display}_f, $display) \\\n");
}
print("\n");

print("#define U8G2_DISPLAY_TABLE_SPI \\\n");
foreach my $display (@spi_displays) {
    print("  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_${display}_f, $display) \\\n");
}
print("\n");

print << 'FOOTER';

#endif /* _U8G2_DISPLAYS_H */
FOOTER
