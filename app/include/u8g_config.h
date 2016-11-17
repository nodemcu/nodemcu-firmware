#ifndef __U8G_CONFIG_H__
#define __U8G_CONFIG_H__


// ***************************************************************************
// Configure U8glib fonts
//
// Add a U8G_FONT_TABLE_ENTRY for each font you want to compile into the image
#define U8G_FONT_TABLE_ENTRY(font)
#define U8G_FONT_TABLE                          \
    U8G_FONT_TABLE_ENTRY(font_6x10)             \
    U8G_FONT_TABLE_ENTRY(font_chikita)
#undef U8G_FONT_TABLE_ENTRY
//
// ***************************************************************************


// ***************************************************************************
// Enable display drivers
//
// Uncomment the U8G_DISPLAY_TABLE_ENTRY for the device(s) you want to
// compile into the firmware.
// Stick to the assignments to *_I2C and *_SPI tables.
//
// I2C based displays go into here:
//    U8G_DISPLAY_TABLE_ENTRY(sh1106_128x64_i2c)          \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1306_128x32_i2c)         \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1306_128x64_i2c)         \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1306_64x48_i2c)          \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1309_128x64_i2c)         \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1327_96x96_gr_i2c)       \
//    U8G_DISPLAY_TABLE_ENTRY(uc1611_dogm240_i2c)         \
//    U8G_DISPLAY_TABLE_ENTRY(uc1611_dogxl240_i2c)        \

#define U8G_DISPLAY_TABLE_ENTRY(device)
#define U8G_DISPLAY_TABLE_I2C                           \
    U8G_DISPLAY_TABLE_ENTRY(ssd1306_128x64_i2c)         \

// SPI based displays go into here:
//    U8G_DISPLAY_TABLE_ENTRY(ld7032_60x32_hw_spi)                \
//    U8G_DISPLAY_TABLE_ENTRY(pcd8544_84x48_hw_spi)               \
//    U8G_DISPLAY_TABLE_ENTRY(pcf8812_96x65_hw_spi)               \
//    U8G_DISPLAY_TABLE_ENTRY(sh1106_128x64_hw_spi)               \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1306_128x32_hw_spi)              \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1306_128x64_hw_spi)              \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1306_64x48_hw_spi)               \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1309_128x64_hw_spi)              \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1322_nhd31oled_bw_hw_spi)        \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1322_nhd31oled_gr_hw_spi)        \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1325_nhd27oled_bw_hw_spi)        \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1325_nhd27oled_gr_hw_spi)        \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1327_96x96_gr_hw_spi)            \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1351_128x128_332_hw_spi)         \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1351_128x128gh_332_hw_spi)       \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1351_128x128_hicolor_hw_spi)     \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1351_128x128gh_hicolor_hw_spi)   \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1353_160x128_332_hw_spi)         \
//    U8G_DISPLAY_TABLE_ENTRY(ssd1353_160x128_hicolor_hw_spi)     \
//    U8G_DISPLAY_TABLE_ENTRY(st7565_64128n_hw_spi)               \
//    U8G_DISPLAY_TABLE_ENTRY(st7565_dogm128_hw_spi)              \
//    U8G_DISPLAY_TABLE_ENTRY(st7565_dogm132_hw_spi)              \
//    U8G_DISPLAY_TABLE_ENTRY(st7565_lm6059_hw_spi)               \
//    U8G_DISPLAY_TABLE_ENTRY(st7565_lm6063_hw_spi)               \
//    U8G_DISPLAY_TABLE_ENTRY(st7565_nhd_c12832_hw_spi)           \
//    U8G_DISPLAY_TABLE_ENTRY(st7565_nhd_c12864_hw_spi)           \
//    U8G_DISPLAY_TABLE_ENTRY(uc1601_c128032_hw_spi)              \
//    U8G_DISPLAY_TABLE_ENTRY(uc1608_240x128_hw_spi)              \
//    U8G_DISPLAY_TABLE_ENTRY(uc1608_240x64_hw_spi)               \
//    U8G_DISPLAY_TABLE_ENTRY(uc1610_dogxl160_bw_hw_spi)          \
//    U8G_DISPLAY_TABLE_ENTRY(uc1610_dogxl160_gr_hw_spi)          \
//    U8G_DISPLAY_TABLE_ENTRY(uc1611_dogm240_hw_spi)              \
//    U8G_DISPLAY_TABLE_ENTRY(uc1611_dogxl240_hw_spi)             \
//    U8G_DISPLAY_TABLE_ENTRY(uc1701_dogs102_hw_spi)              \
//    U8G_DISPLAY_TABLE_ENTRY(uc1701_mini12864_hw_spi)            \

#define U8G_DISPLAY_TABLE_SPI                                   \
    U8G_DISPLAY_TABLE_ENTRY(ssd1306_128x64_hw_spi)              \

#undef U8G_DISPLAY_TABLE_ENTRY

// Special display device to provide run-length encoded framebuffer contents
// to a Lua callback:
//#define U8G_DISPLAY_FB_RLE
//
// ***************************************************************************


#endif	/* __U8G_CONFIG_H__ */
