#ifndef __UCG_CONFIG_H__
#define __UCG_CONFIG_H__


// ***************************************************************************
// Configure Ucglib fonts
//
// Add a UCG_FONT_TABLE_ENTRY for each font you want to compile into the image
#define UCG_FONT_TABLE_ENTRY(font)
#define UCG_FONT_TABLE                              \
    UCG_FONT_TABLE_ENTRY(font_7x13B_tr)             \
    UCG_FONT_TABLE_ENTRY(font_helvB08_hr)           \
    UCG_FONT_TABLE_ENTRY(font_helvB10_hr)           \
    UCG_FONT_TABLE_ENTRY(font_helvB12_hr)           \
    UCG_FONT_TABLE_ENTRY(font_helvB18_hr)           \
    UCG_FONT_TABLE_ENTRY(font_ncenB24_tr)           \
    UCG_FONT_TABLE_ENTRY(font_ncenR12_tr)           \
    UCG_FONT_TABLE_ENTRY(font_ncenR14_hr)
#undef UCG_FONT_TABLE_ENTRY
//
// ***************************************************************************


// ***************************************************************************
// Enable display drivers
//
// Uncomment the UCG_DISPLAY_TABLE_ENTRY for the device(s) you want to
// compile into the firmware.
//
//    UCG_DISPLAY_TABLE_ENTRY(ili9163_18x128x128_hw_spi, ucg_dev_ili9163_18x128x128, ucg_ext_ili9163_18) \
//    UCG_DISPLAY_TABLE_ENTRY(ili9341_18x240x320_hw_spi, ucg_dev_ili9341_18x240x320, ucg_ext_ili9341_18) \
//    UCG_DISPLAY_TABLE_ENTRY(pcf8833_16x132x132_hw_spi, ucg_dev_pcf8833_16x132x132, ucg_ext_pcf8833_16) \
//    UCG_DISPLAY_TABLE_ENTRY(seps225_16x128x128_uvis_hw_spi, ucg_dev_seps225_16x128x128_univision, ucg_ext_seps225_16) \
//    UCG_DISPLAY_TABLE_ENTRY(ssd1351_18x128x128_hw_spi, ucg_dev_ssd1351_18x128x128_ilsoft, ucg_ext_ssd1351_18) \
//    UCG_DISPLAY_TABLE_ENTRY(ssd1351_18x128x128_ft_hw_spi, ucg_dev_ssd1351_18x128x128_ft, ucg_ext_ssd1351_18) \
//    UCG_DISPLAY_TABLE_ENTRY(ssd1331_18x96x64_uvis_hw_spi, ucg_dev_ssd1331_18x96x64_univision, ucg_ext_ssd1331_18) \
//    UCG_DISPLAY_TABLE_ENTRY(st7735_18x128x160_hw_spi, ucg_dev_st7735_18x128x160, ucg_ext_st7735_18) \

#define UCG_DISPLAY_TABLE_ENTRY(binding, device, extension)
#define UCG_DISPLAY_TABLE \
    UCG_DISPLAY_TABLE_ENTRY(ili9341_18x240x320_hw_spi, ucg_dev_ili9341_18x240x320, ucg_ext_ili9341_18) \
    UCG_DISPLAY_TABLE_ENTRY(st7735_18x128x160_hw_spi, ucg_dev_st7735_18x128x160, ucg_ext_st7735_18) \

#undef UCG_DISPLAY_TABLE_ENTRY
//
// ***************************************************************************


#endif	/* __UCG_CONFIG_H__ */
