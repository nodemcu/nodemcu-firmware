-- ***************************************************************************
-- Graphics Test
--
-- This script executes several features of u8glib to test their Lua bindings.
--
-- Note: It is prepared for SSD1306-based displays. Select your connectivity
--       type by calling either init_i2c_display() or init_spi_display() at
--       the bottom of this file.
--
-- ***************************************************************************

-- setup I2c and connect display
function init_i2c_display()
    -- SDA and SCL can be assigned freely to available GPIOs
    local sda = 5 -- GPIO14
    local scl = 6 -- GPIO12
    local sla = 0x3c
    i2c.setup(0, sda, scl, i2c.SLOW)
    disp = u8g2.ssd1306_i2c_128x64_noname(0, sla)
end

-- setup SPI and connect display
function init_spi_display()
    -- Hardware SPI CLK  = GPIO14
    -- Hardware SPI MOSI = GPIO13
    -- Hardware SPI MISO = GPIO12 (not used)
    -- Hardware SPI /CS  = GPIO15 (not used)
    -- CS, D/C, and RES can be assigned freely to available GPIOs
    local cs  = 8 -- GPIO15, pull-down 10k to GND
    local dc  = 4 -- GPIO2
    local res = 0 -- GPIO16

    spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, 8, 8)
    -- we won't be using the HSPI /CS line, so disable it again
    gpio.mode(8, gpio.INPUT, gpio.PULLUP)

    disp = u8g2.ssd1306_128x64_noname(1, cs, dc, res)
end


function u8g2_prepare()
  disp:setFont(u8g2.font_6x10_tf)
  disp:setFontRefHeightExtendedText()
  disp:setDrawColor(1)
  disp:setFontPosTop()
  disp:setFontDirection(0)
end


function u8g2_box_frame(a)
  disp:drawStr( 0, 0, "drawBox")
  disp:drawBox(5,10,20,10)
  disp:drawBox(10+a,15,30,7)
  disp:drawStr( 0, 30, "drawFrame")
  disp:drawFrame(5,10+30,20,10)
  disp:drawFrame(10+a,15+30,30,7)
end

function u8g2_disc_circle(a)
  disp:drawStr( 0, 0, "drawDisc")
  disp:drawDisc(10,18,9)
  disp:drawDisc(24+a,16,7)
  disp:drawStr( 0, 30, "drawCircle")
  disp:drawCircle(10,18+30,9)
  disp:drawCircle(24+a,16+30,7)
end

function u8g2_r_frame(a)
  disp:drawStr( 0, 0, "drawRFrame/Box")
  disp:drawRFrame(5, 10,40,30, a+1)
  disp:drawRBox(50, 10,25,40, a+1)
end

function u8g2_string(a)
  disp:setFontDirection(0)
  disp:drawStr(30+a,31, " 0")
  disp:setFontDirection(1)
  disp:drawStr(30,31+a, " 90")
  disp:setFontDirection(2)
  disp:drawStr(30-a,31, " 180")
  disp:setFontDirection(3)
  disp:drawStr(30,31-a, " 270")
end

function u8g2_line(a)
  disp:drawStr( 0, 0, "drawLine")
  disp:drawLine(7+a, 10, 40, 55)
  disp:drawLine(7+a*2, 10, 60, 55)
  disp:drawLine(7+a*3, 10, 80, 55)
  disp:drawLine(7+a*4, 10, 100, 55)
end

function u8g2_triangle(a)
  local offset = a
  disp:drawStr( 0, 0, "drawTriangle")
  disp:drawTriangle(14,7, 45,30, 10,40)
  disp:drawTriangle(14+offset,7-offset, 45+offset,30-offset, 57+offset,10-offset)
  disp:drawTriangle(57+offset*2,10, 45+offset*2,30, 86+offset*2,53)
  disp:drawTriangle(10+offset,40+offset, 45+offset,30+offset, 86+offset,53+offset)
end

function u8g2_ascii_1()
  disp:drawStr( 0, 0, "ASCII page 1")
  for y = 0, 5 do
    for x = 0, 15 do
      disp:drawStr(x*7, y*10+10, string.char(y*16 + x + 32))
    end
  end
end

function u8g2_ascii_2()
  disp:drawStr( 0, 0, "ASCII page 2")
  for y = 0, 5 do
    for x = 0, 15 do
      disp:drawStr(x*7, y*10+10, string.char(y*16 + x + 160))
    end
  end
end

function u8g2_extra_page(a)
  disp:drawStr( 0, 0, "Unicode")
  disp:setFont(u8g2.font_unifont_t_symbols)
  disp:setFontPosTop()
  disp:drawUTF8(0, 24, "☀ ☁")
  if a <= 3 then
    disp:drawUTF8(a*3, 36, "☂")
  else
    disp:drawUTF8(a*3, 36, "☔")
  end
end

cross_width = 24
cross_height = 24
cross_bits = string.char(
  0x00, 0x18, 0x00, 0x00, 0x24, 0x00, 0x00, 0x24, 0x00, 0x00, 0x42, 0x00, 
  0x00, 0x42, 0x00, 0x00, 0x42, 0x00, 0x00, 0x81, 0x00, 0x00, 0x81, 0x00, 
  0xC0, 0x00, 0x03, 0x38, 0x3C, 0x1C, 0x06, 0x42, 0x60, 0x01, 0x42, 0x80, 
  0x01, 0x42, 0x80, 0x06, 0x42, 0x60, 0x38, 0x3C, 0x1C, 0xC0, 0x00, 0x03, 
  0x00, 0x81, 0x00, 0x00, 0x81, 0x00, 0x00, 0x42, 0x00, 0x00, 0x42, 0x00, 
  0x00, 0x42, 0x00, 0x00, 0x24, 0x00, 0x00, 0x24, 0x00, 0x00, 0x18, 0x00)

cross_fill_width = 24
cross_fill_height = 24
cross_fill_bits = string.char(
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x18, 0x64, 0x00, 0x26, 
  0x84, 0x00, 0x21, 0x08, 0x81, 0x10, 0x08, 0x42, 0x10, 0x10, 0x3C, 0x08, 
  0x20, 0x00, 0x04, 0x40, 0x00, 0x02, 0x80, 0x00, 0x01, 0x80, 0x18, 0x01, 
  0x80, 0x18, 0x01, 0x80, 0x00, 0x01, 0x40, 0x00, 0x02, 0x20, 0x00, 0x04, 
  0x10, 0x3C, 0x08, 0x08, 0x42, 0x10, 0x08, 0x81, 0x10, 0x84, 0x00, 0x21, 
  0x64, 0x00, 0x26, 0x18, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)

cross_block_width = 14
cross_block_height = 14
cross_block_bits = string.char(
  0xFF, 0x3F, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 
  0xC1, 0x20, 0xC1, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 
  0x01, 0x20, 0xFF, 0x3F)

function u8g2_bitmap_overlay(a)
  local frame_size = 28

  disp:drawStr(0, 0, "Bitmap overlay")

  disp:drawStr(0, frame_size + 12, "Solid / transparent")
  disp:setBitmapMode(0) -- solid
  disp:drawFrame(0, 10, frame_size, frame_size)
  disp:drawXBM(2, 12, cross_width, cross_height, cross_bits)
  if bit.band(a, 4) > 0 then
    disp:drawXBM(7, 17, cross_block_width, cross_block_height, cross_block_bits)
  end

  disp:setBitmapMode(1) -- transparent
  disp:drawFrame(frame_size + 5, 10, frame_size, frame_size)
  disp:drawXBM(frame_size + 7, 12, cross_width, cross_height, cross_bits)
  if bit.band(a, 4) then
    disp:drawXBM(frame_size + 12, 17, cross_block_width, cross_block_height, cross_block_bits)
  end
end

function u8g2_bitmap_modes(transparent)
  local frame_size = 24

  disp:drawBox(0, frame_size * 0.5, frame_size * 5, frame_size)
  disp:drawStr(frame_size * 0.5, 50, "Black")
  disp:drawStr(frame_size * 2, 50, "White")
  disp:drawStr(frame_size * 3.5, 50, "XOR")
  
  if transparent == 0 then
    disp:setBitmapMode(0) -- solid
    disp:drawStr(0, 0, "Solid bitmap")
  else
    disp:setBitmapMode(1) -- transparent
    disp:drawStr(0, 0, "Transparent bitmap")
  end
  disp:setDrawColor(0) -- Black
  disp:drawXBM(frame_size * 0.5, 24, cross_width, cross_height, cross_bits)
  disp:setDrawColor(1) -- White
  disp:drawXBM(frame_size * 2, 24, cross_width, cross_height, cross_bits)
  disp:setDrawColor(2) -- XOR
  disp:drawXBM(frame_size * 3.5, 24, cross_width, cross_height, cross_bits)
end


function draw()
  u8g2_prepare()

  local d3 = bit.rshift(draw_state, 3)
  local d7 = bit.band(draw_state, 7)

  if d3 == 0 then
    u8g2_box_frame(d7)
  elseif d3 == 1 then
    u8g2_disc_circle(d7)
  elseif d3 == 2 then
    u8g2_r_frame(d7)
  elseif d3 == 3 then
    u8g2_string(d7)
  elseif d3 == 4 then
    u8g2_line(d7)
  elseif d3 == 5 then
    u8g2_triangle(d7)
  elseif d3 == 6 then
    u8g2_ascii_1()
  elseif d3 == 7 then
    u8g2_ascii_2()
  elseif d3 == 8 then
    u8g2_extra_page(d7)
  elseif d3 == 9 then
    u8g2_bitmap_modes(0)
  elseif d3 == 10 then
    u8g2_bitmap_modes(1)
  elseif d3 == 11 then
    u8g2_bitmap_overlay(d7)
  end
end


function loop()
  -- picture loop  
  disp:clearBuffer()
  draw()
  disp:sendBuffer()
  
  -- increase the state
  draw_state = draw_state + 1
  if draw_state >= 12*8 then
    draw_state = 0
  end

  -- delay between each frame
  loop_tmr:start()
end


draw_state = 0
loop_tmr = tmr.create()
loop_tmr:register(100, tmr.ALARM_SEMI, loop)

init_i2c_display()
--init_spi_display()

print("--- Starting Graphics Test ---")
loop_tmr:start()
