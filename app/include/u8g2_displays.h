
#ifndef _U8G2_DISPLAYS_H
#define _U8G2_DISPLAYS_H

#define U8G2_DISPLAY_TABLE_ENTRY(function, binding)

// ***************************************************************************
// Enable display drivers
//
// Uncomment the U8G2_DISPLAY_TABLE_ENTRY for the device(s) you want to
// compile into the firmware.
// Stick to the assignments to *_I2C and *_SPI tables.

#ifndef U8G2_DISPLAY_TABLE_I2C_EXTRA

// I2C based displays go into here:
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1610_i2c_ea_dogxl160_f, uc1610_i2c_ea_dogxl160) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1325_i2c_nhd_128x64_f, ssd1325_i2c_nhd_128x64) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_i2c_64x48_er_f, ssd1306_i2c_64x48_er) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1608_i2c_erc24064_f, uc1608_i2c_erc24064) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7588_i2c_jlx12864_f, st7588_i2c_jlx12864) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1309_i2c_128x64_noname0_f, ssd1309_i2c_128x64_noname0) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1611_i2c_ea_dogxl240_f, uc1611_i2c_ea_dogxl240) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1305_i2c_128x32_noname_f, ssd1305_i2c_128x32_noname) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_i2c_128x32_univision_f, ssd1306_i2c_128x32_univision) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1608_i2c_240x128_f, uc1608_i2c_240x128) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ld7032_i2c_60x32_f, ld7032_i2c_60x32) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1611_i2c_ew50850_f, uc1611_i2c_ew50850) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1604_i2c_jlx19264_f, uc1604_i2c_jlx19264) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1601_i2c_128x32_f, uc1601_i2c_128x32) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_sh1106_i2c_128x64_vcomh0_f, sh1106_i2c_128x64_vcomh0) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_i2c_96x16_er_f, ssd1306_i2c_96x16_er) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_sh1106_i2c_128x64_noname_f, sh1106_i2c_128x64_noname) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_i2c_128x64_vcomh0_f, ssd1306_i2c_128x64_vcomh0) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_i2c_128x64_noname_f, ssd1306_i2c_128x64_noname) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1309_i2c_128x64_noname2_f, ssd1309_i2c_128x64_noname2) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1611_i2c_ea_dogm240_f, uc1611_i2c_ea_dogm240) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1327_i2c_seeed_96x96_f, ssd1327_i2c_seeed_96x96) \

#define U8G2_DISPLAY_TABLE_I2C \
  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_i2c_128x64_noname_f, ssd1306_i2c_128x64_noname) \

#else

// I2C displays can be defined in an external file. 
#define U8G2_DISPLAY_TABLE_I2C \
   U8G2_DISPLAY_TABLE_I2C_EXTRA

#endif


#ifndef U8G2_DISPLAY_TABLE_SPI_EXTRA

// SPI based displays go into here:
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1606_172x72_f, ssd1606_172x72) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1608_240x128_f, uc1608_240x128) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7565_erc12864_f, st7565_erc12864) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1309_128x64_noname0_f, ssd1309_128x64_noname0) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1601_128x32_f, uc1601_128x32) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1608_erc24064_f, uc1608_erc24064) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7565_lm6059_f, st7565_lm6059) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1611_ea_dogxl240_f, uc1611_ea_dogxl240) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7565_nhd_c12864_f, st7565_nhd_c12864) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_128x64_vcomh0_f, ssd1306_128x64_vcomh0) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1305_128x32_noname_f, ssd1305_128x32_noname) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_max7219_32x8_f, max7219_32x8) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ls013b7dh03_128x128_f, ls013b7dh03_128x128) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_il3820_v2_296x128_f, il3820_v2_296x128) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1610_ea_dogxl160_f, uc1610_ea_dogxl160) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1611_ea_dogm240_f, uc1611_ea_dogm240) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1604_jlx19264_f, uc1604_jlx19264) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7920_s_192x32_f, st7920_s_192x32) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1325_nhd_128x64_f, ssd1325_nhd_128x64) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_128x64_noname_f, ssd1306_128x64_noname) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_sed1520_122x32_f, sed1520_122x32) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7565_ea_dogm128_f, st7565_ea_dogm128) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ld7032_60x32_f, ld7032_60x32) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1607_200x200_f, ssd1607_200x200) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1309_128x64_noname2_f, ssd1309_128x64_noname2) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_sh1106_128x64_noname_f, sh1106_128x64_noname) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_128x32_univision_f, ssd1306_128x32_univision) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7920_s_128x64_f, st7920_s_128x64) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7565_64128n_f, st7565_64128n) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1701_ea_dogs102_f, uc1701_ea_dogs102) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1611_ew50850_f, uc1611_ew50850) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1322_nhd_256x64_f, ssd1322_nhd_256x64) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7565_ea_dogm132_f, st7565_ea_dogm132) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1329_128x96_noname_f, ssd1329_128x96_noname) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7565_zolen_128x64_f, st7565_zolen_128x64) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st75256_jlx256128_f, st75256_jlx256128) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_96x16_er_f, ssd1306_96x16_er) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ist3020_erc19264_f, ist3020_erc19264) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7588_jlx12864_f, st7588_jlx12864) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1327_seeed_96x96_f, ssd1327_seeed_96x96) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st75256_jlx172104_f, st75256_jlx172104) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7565_nhd_c12832_f, st7565_nhd_c12832) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_64x48_er_f, ssd1306_64x48_er) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_pcf8812_96x65_f, pcf8812_96x65) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7567_pi_132x64_f, st7567_pi_132x64) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_st7567_jlx12864_f, st7567_jlx12864) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_pcd8544_84x48_f, pcd8544_84x48) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_sh1106_128x64_vcomh0_f, sh1106_128x64_vcomh0) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_nt7534_tg12864r_f, nt7534_tg12864r) \
//  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_uc1701_mini12864_f, uc1701_mini12864) \

#define U8G2_DISPLAY_TABLE_SPI \
  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_128x64_noname_f, ssd1306_128x64_noname) \

#else

// SPI displays can be defined in an external file. 
#define U8G2_DISPLAY_TABLE_SPI \
   U8G2_DISPLAY_TABLE_SPI_EXTRA

#endif

//
// ***************************************************************************


#endif /* _U8G2_DISPLAYS_H */
