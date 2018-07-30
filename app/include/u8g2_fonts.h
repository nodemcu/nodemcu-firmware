
#ifndef _U8G2_FONTS_H
#define _U8G2_FONTS_H

#define U8G2_FONT_TABLE_ENTRY(font)

// ***************************************************************************
// Configure U8glib fonts
//
#ifndef U8G2_FONT_TABLE_EXTRA
//
// Add a U8G2_FONT_TABLE_ENTRY for each font you want to compile into the image
// See https://github.com/olikraus/u8g2/wiki/fntlistall for a complete list of
// available fonts. Drop the 'u8g2_' prefix when you add them here.
#define U8G2_FONT_TABLE \
  U8G2_FONT_TABLE_ENTRY(font_6x10_tf) \
  U8G2_FONT_TABLE_ENTRY(font_unifont_t_symbols) \

#else
//
// The font table can be defined in an external file. 
#define U8G2_FONT_TABLE \
  U8G2_FONT_TABLE_EXTRA

#endif
// ***************************************************************************


#endif /* _U8G2_FONTS_H */
