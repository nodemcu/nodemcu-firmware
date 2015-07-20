#ifndef __U8G_CONFIG_H__
#define __U8G_CONFIG_H__


// ***************************************************************************
// Configure U8glib fonts
//
// Add a U8G_FONT_TABLE_ENTRY for each font you want to compile into the image
#define U8G_FONT_TABLE_ENTRY(font)
#define U8G_FONT_TABLE \
    U8G_FONT_TABLE_ENTRY(font_6x10)  \
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
#define U8G_DISPLAY_TABLE_ENTRY(device, lua_api_name)
#define U8G_DISPLAY_TABLE_I2C \
    U8G_DISPLAY_TABLE_ENTRY(ssd1306_128x64_i2c, ssd1306_128x64_i2c)

// SPI based displays go into here:
#define U8G_DISPLAY_TABLE_SPI \
    U8G_DISPLAY_TABLE_ENTRY(ssd1306_128x64_hw_spi, ssd1306_128x64_spi)  \
//    U8G_DISPLAY_TABLE_ENTRY(pcd8544_84x48_hw_spi, pcd8544_84x48_spi)
#undef U8G_DISPLAY_TABLE_ENTRY
//
// ***************************************************************************


#endif	/* __U8G_CONFIG_H__ */
