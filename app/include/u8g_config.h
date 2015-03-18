#ifndef __U8G_CONFIG_H__
#define __U8G_CONFIG_H__


// Configure U8glib fonts
// add a U8G_FONT_TABLE_ENTRY for each font you want to compile into the image
#define U8G_FONT_TABLE_ENTRY(font)
#define U8G_FONT_TABLE \
    U8G_FONT_TABLE_ENTRY(font_6x10)  \
    U8G_FONT_TABLE_ENTRY(font_chikita)
#undef U8G_FONT_TABLE_ENTRY


// Enable display drivers
#define U8G_SSD1306_128x64_I2C
#define U8G_SSD1306_128x64_SPI
// untested
#undef U8G_PCD8544_84x48


#endif	/* __U8G_CONFIG_H__ */
