
-- setup I2c and connect display
function init_i2c_display()
     sda = 5
     scl = 6
     sla = 0x3c
     i2c.setup(0, sda, scl, i2c.SLOW)
     disp = u8g.ssd1306_128x64_i2c(sla)
end

function xbm_picture()
     disp:setFont(u8g.font_6x10)
     disp:drawStr( 0, 10, "XBM picture")

     disp:drawXBM( 0, 20, 38, 24, xbm_data )
end

function bitmap_picture(state)
     disp:setFont(u8g.font_6x10)
     disp:drawStr( 0, 10, "Bitmap picture")

     disp:drawBitmap( 0 + (state * 10), 20 + (state * 4), 1, 8, bm_data )
end

-- the draw() routine
function draw(draw_state)
     local component = bit.rshift(draw_state, 3)

     if (component == 0) then
          xbm_picture(bit.band(draw_state, 7))
     elseif (component == 1) then
          bitmap_picture(bit.band(draw_state, 7))
     end
end


function bitmap_test()
     init_i2c_display()

     -- read XBM picture
     file.open("u8glib_logo.xbm", "r")
     xbm_data = file.read()
     file.close()

     -- read Bitmap picture
     file.open("u8g_rook.bm", "r")
     bm_data = file.read()
     file.close()

     print("--- Starting Bitmap Test ---")
     dir = 0
     next_rotation = 0

     local draw_state
     for draw_state = 1, 7 + 1*8, 1 do
          disp:firstPage()
          repeat
               draw(draw_state)
          until disp:nextPage() == false

          tmr.wdclr()
     end

     print("--- Bitmap Test done ---")
end

bitmap_test()
