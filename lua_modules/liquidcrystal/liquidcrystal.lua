local bit = bit --luacheck: read globals bit
-- metatable
local LiquidCrystal = {}
LiquidCrystal.__index = LiquidCrystal

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


function LiquidCrystal:autoscroll(on)
   if on then
      self._displaymode = bit.bor(self._displaymode, LCD_ENTRYSHIFTINCREMENT)
   else
      self._displaymode = bit.band(self._displaymode, bit.bnot(LCD_ENTRYSHIFTINCREMENT))
   end
   return self:_command(bit.bor(LCD_ENTRYMODESET, self._displaymode))
end

function LiquidCrystal:blink(on)
   if on then
      self._displaycontrol = bit.bor(self._displaycontrol, LCD_BLINKON)
   else
      self._displaycontrol = bit.band(self._displaycontrol, bit.bnot(LCD_BLINKON))
   end
   return self:_command(bit.bor(LCD_DISPLAYCONTROL, self._displaycontrol))
end

function LiquidCrystal:clear() return self:_command(LCD_CLEARDISPLAY) end

function LiquidCrystal:cursorLeft()
   return self:_command(bit.bor(LCD_CURSORSHIFT, LCD_CURSORMOVE, LCD_MOVELEFT))
end

function LiquidCrystal:cursorMove(col, row)
   return self:_command(bit.bor(LCD_SETDDRAMADDR, col + (row and (self._offsets[row] - 1) or 0)))
end

function LiquidCrystal:cursor(on)
   if on then
      self._displaycontrol = bit.bor(self._displaycontrol, LCD_CURSORON)
   else
      self._displaycontrol = bit.band(self._displaycontrol, bit.bnot(LCD_CURSORON))
   end
   return self:_command(bit.bor(LCD_DISPLAYCONTROL, self._displaycontrol))
end

function LiquidCrystal:cursorRight()
   return self:_command(bit.bor(LCD_CURSORSHIFT, LCD_CURSORMOVE, LCD_MOVERIGHT))
end

function LiquidCrystal:customChar(index, bytes)
   local pos = self:position()
   self:_command(bit.bor(LCD_SETCGRAMADDR,
                         bit.lshift(bit.band(self._eightdots and index or bit.clear(index, 0),
                                             0x7), 3)))
   for i=1,(self._eightdots and 8 or 11) do self:_write(bytes[i] or 0) end
   self:cursorMove(pos)
end

function LiquidCrystal:display(on)
   if on then
      self._displaycontrol = bit.bor(self._displaycontrol, LCD_DISPLAYON)
   else
      self._displaycontrol = bit.band(self._displaycontrol, bit.bnot(LCD_DISPLAYON))
   end
   return self:_command(bit.bor(LCD_DISPLAYCONTROL, self._displaycontrol))
end

function LiquidCrystal:home() return self:_command(LCD_RETURNHOME) end

function LiquidCrystal:leftToRight()
   self._displaymode = bit.bor(self._displaymode, LCD_ENTRYLEFT)
   return self:_command(bit.bor(LCD_ENTRYMODESET, self._displaymode))
end

function LiquidCrystal:readCustom(index)
   local pos = self:position()
   local data = {}
   self:_command(bit.bor(LCD_SETCGRAMADDR,
                         bit.lshift(bit.band(self._eightdots and index or bit.clear(index, 0),
                                             0x7), 3)))
   for i=1,(self._eightdots and 8 or 11) do data[i] = self:read() end
   self:cursorMove(pos)
   return data
end

function LiquidCrystal:rightToLeft()
   self._displaymode = bit.band(self._displaymode, bit.bnot(LCD_ENTRYLEFT))
   self:_command(bit.bor(LCD_ENTRYMODESET, self._displaymode))
end

function LiquidCrystal:scrollLeft()
   return self:_command(bit.bor(LCD_CURSORSHIFT, LCD_DISPLAYMOVE, LCD_MOVELEFT))
end

function LiquidCrystal:scrollRight()
   return self:_command(bit.bor(LCD_CURSORSHIFT, LCD_DISPLAYMOVE, LCD_MOVERIGHT))
end

function LiquidCrystal:write(...)
   for _, x in ipairs({...}) do
      if type(x) == "number" then
	 self:_write(x)
      end
      if type(x) == "string" then
	 for i=1,#x do
	    self:_write(string.byte(x, i))
	 end
      end
   end
end


return function (backend, onelinemode, eightdotsmode, column_width)
   local self = {}
   setmetatable(self, LiquidCrystal)

   -- copy out backend functions, to avoid a long-lived table
   self._command  = backend.command
   self.busy      = backend.busy
   self.position  = backend.position
   self._write    = backend.write
   self.read      = backend.read
   self.backlight = backend.backlight

   -- defaults
   self._displaycontrol = 0
   self._displaymode = 0

   self._eightdots = eightdotsmode
   self._offsets = {0, 0x40}
   if column_width ~= nil then
      self._offsets[3] =  0    + column_width
      self._offsets[4] =  0x40 + column_width
   end

   self:_command(bit.bor(LCD_FUNCTIONSET,
                         bit.bor(
                            backend.fourbits and LCD_4BITMODE or LCD_8BITMODE,
                            onelinemode      and LCD_1LINE or LCD_2LINE,
                            eightdotsmode    and LCD_5x8DOTS or LCD_5x10DOTS)))
   self:_command(bit.bor(LCD_ENTRYMODESET, self._displaymode))

   self:display(true)
   self:clear()

   return self
end
