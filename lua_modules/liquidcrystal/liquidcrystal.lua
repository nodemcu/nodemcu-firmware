local LiquidCrystal = {}

-- commands
local LCD_CLEARDISPLAY = 0x01
local LCD_RETURNHOME = 0x02
local LCD_ENTRYMODESET = 0x04
local LCD_DISPLAYCONTROL = 0x08
local LCD_CURSORSHIFT = 0x10
local LCD_FUNCTIONSET = 0x20
local LCD_SETCGRAMADDR = 0x40
local LCD_SETDDRAMADDR = 0x80

-- flags for display entry mode
-- local LCD_ENTRYRIGHT = 0x00
local LCD_ENTRYLEFT = 0x02
local LCD_ENTRYSHIFTINCREMENT = 0x01
-- local LCD_ENTRYSHIFTDECREMENT = 0x00

-- flags for display on/off control
local LCD_DISPLAYON = 0x04
-- local LCD_DISPLAYOFF = 0x00
local LCD_CURSORON = 0x02
-- local LCD_CURSOROFF = 0x00
local LCD_BLINKON = 0x01
-- local LCD_BLINKOFF = 0x00

-- flags for display/cursor shift
local LCD_DISPLAYMOVE = 0x08
local LCD_CURSORMOVE = 0x00
local LCD_MOVERIGHT = 0x04
local LCD_MOVELEFT = 0x00

-- flags for function set
local LCD_8BITMODE = 0x10
local LCD_4BITMODE = 0x00
local LCD_2LINE = 0x08
local LCD_1LINE = 0x00
local LCD_5x10DOTS = 0x04
local LCD_5x8DOTS = 0x00

-- defaults
LiquidCrystal._displayfunction = 0
LiquidCrystal._displaycontrol = 0
LiquidCrystal._displaymode = 0
LiquidCrystal._backlight = false

function LiquidCrystal:init(bus, bus_args, fourbitmode, onelinemode, eightdotsmode, column_width, insert_delay)
   self._bus_args = bus_args
   if bus == "I2C" then
      assert(fourbitmode, "8bit mode not supported")
      self._backend = require "i2c4bit"
   elseif bus == "GPIO" then
      if fourbitmode then
	 self._backend = require "gpio4bit"
      else
	 self._backend = require "gpio8bit"
      end
   else
      error(string.format("%s backend is not implemented", bus))
   end

   if fourbitmode then
      self._displayfunction = bit.bor(self._displayfunction, LCD_4BITMODE)
   else
      self._displayfunction = bit.bor(self._displayfunction, LCD_8BITMODE)
   end

   if onelinemode then
      self._displayfunction = bit.bor(self._displayfunction, LCD_1LINE)
   else
      self._displayfunction = bit.bor(self._displayfunction, LCD_2LINE)
   end

   if eightdotsmode then
      self._displayfunction = bit.bor(self._displayfunction, LCD_5x8DOTS)
   else
      self._displayfunction = bit.bor(self._displayfunction, LCD_5x10DOTS)
   end

   self._offsets = {0, 0x40}
   if column_width ~= nil then
      self._offsets[3] =  0 + column_width
      self._offsets[4] =  0x40 + column_width
   end
   self._insert_delay = insert_delay

   self._backend.init(self)
   self._backend.command(self, bit.bor(LCD_FUNCTIONSET, self._displayfunction))
   self._backend.command(self, bit.bor(LCD_ENTRYMODESET, self._displaymode))

   self:display(true)
   self:clear()
end

function LiquidCrystal:clear()
   self._backend.command(self, LCD_CLEARDISPLAY)
   if self._insert_delay then
      tmr.delay(2000)
   end
end

function LiquidCrystal:home()
   self._backend.command(self, LCD_RETURNHOME)
   if self._insert_delay then
      tmr.delay(2000)
   end
end

function LiquidCrystal:cursorMove(col, row)
   if row ~= nil then
      self._backend.command(self, bit.bor(LCD_SETDDRAMADDR, col + self._offsets[row] - 1))
   else
      self._backend.command(self, bit.bor(LCD_SETDDRAMADDR, col))
   end
end

function LiquidCrystal:display(on)
   if on then
      self._displaycontrol = bit.bor(self._displaycontrol, LCD_DISPLAYON)
   else
         self._displaycontrol = bit.band(self._displaycontrol, bit.bnot(LCD_DISPLAYON))
   end
   self._backend.command(self, bit.bor(LCD_DISPLAYCONTROL, self._displaycontrol))
end

function LiquidCrystal:blink(on)
   if on then
      self._displaycontrol = bit.bor(self._displaycontrol, LCD_BLINKON)
   else
      self._displaycontrol = bit.band(self._displaycontrol, bit.bnot(LCD_BLINKON))
   end
   self._backend.command(self, bit.bor(LCD_DISPLAYCONTROL, self._displaycontrol))
end

function LiquidCrystal:cursor(on)
   if on then
      self._displaycontrol = bit.bor(self._displaycontrol, LCD_CURSORON)
   else
      self._displaycontrol = bit.band(self._displaycontrol, bit.bnot(LCD_CURSORON))
   end
   self._backend.command(self, bit.bor(LCD_DISPLAYCONTROL, self._displaycontrol))
end

function LiquidCrystal:cursorLeft()
   self._backend.command(self, bit.bor(LCD_CURSORSHIFT, LCD_CURSORMOVE, LCD_MOVELEFT))
end

function LiquidCrystal:cursorRight()
   self._backend.command(self, bit.bor(LCD_CURSORSHIFT, LCD_CURSORMOVE, LCD_MOVERIGHT))
end

function LiquidCrystal:scrollLeft()
   self._backend.command(self, bit.bor(LCD_CURSORSHIFT, LCD_DISPLAYMOVE, LCD_MOVELEFT))
end

function LiquidCrystal:scrollRight()
   self._backend.command(self, bit.bor(LCD_CURSORSHIFT, LCD_DISPLAYMOVE, LCD_MOVERIGHT))
end

function LiquidCrystal:leftToRight()
   self._displaymode = bit.bor(self._displaymode, LCD_ENTRYLEFT)
   self._backend.command(self, bit.bor(LCD_ENTRYMODESET, self._displaymode))
end

function LiquidCrystal:rightToLeft()
   self._displaymode = bit.band(self._displaymode, bit.bnot(LCD_ENTRYLEFT))
   self._backend.command(self, bit.bor(LCD_ENTRYMODESET, self._displaymode))
end

function LiquidCrystal:autoscroll(on)
   if on then
      self._displaymode = bit.bor(self._displaymode, LCD_ENTRYSHIFTINCREMENT)
   else
      self._displaymode = bit.band(self._displaymode, bit.bnot(LCD_ENTRYSHIFTINCREMENT))
   end
   self._backend.command(self, bit.bor(LCD_ENTRYMODESET, self._displaymode))
end

function LiquidCrystal:write(...)
   for _, x in ipairs({...}) do
      if type(x) == "number" then
	 self._backend.write(self, x)
      end
      if type(x) == "string" then
	 for i=1,#x do
	    self._backend.write(self, string.byte(x, i))
	    if self._insert_delay then
	       tmr.delay(800)
	    end
	 end
      end
      if self._insert_delay then
	 tmr.delay(800)
      end
   end
end

function LiquidCrystal:customChar(index, bytes)
   self._backend.command(self, bit.bor(LCD_SETCGRAMADDR, bit.lshift(bit.band(index, 0x7), 3)))
   for _, b in ipairs(bytes) do
      self._backend.write(self, b)
      if self._insert_delay then
	 tmr.delay(800)
      end
   end
   self:home()
end

function LiquidCrystal:backlight(on)
   self._backend.backlight(self, on)
end

return LiquidCrystal
