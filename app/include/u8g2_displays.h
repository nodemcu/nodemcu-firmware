
#ifndef _U8G2_DISPLAYS_H
#define _U8G2_DISPLAYS_H

#define U8G2_DISPLAY_TABLE_ENTRY(function, binding)

#define U8G2_DISPLAY_TABLE_I2C \
  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_i2c_128x64_noname_f, ssd1306_i2c_128x64_noname) \

#define U8G2_DISPLAY_TABLE_SPI \
  U8G2_DISPLAY_TABLE_ENTRY(u8g2_Setup_ssd1306_128x64_noname_f, ssd1306_128x64_noname) \


#endif /* _U8G2_DISPLAYS_H */
