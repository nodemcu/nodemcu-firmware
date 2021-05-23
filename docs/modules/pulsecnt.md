# Pulse Counter Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-04-07 | [ChiliPeppr](https://github.com/chilipeppr) | John Lauer | [pulsecnt.c](../../components/modules/pulsecnt.c)|

The pulsecnt library handles sophisticated and automatic pulse counting using the built-in hardware on ESP32.
The pulsecnt library gives you a means to not rely on GPIO triggers to do your pulse counting and instead offload the work to independent hardware. You gain the ability to count pulses up to 80Mhz (speed of APB clock).
You can get a callback to Lua when different counting thresholds are reached, or when upper or lower counting limits are hit. You can count pulses on all GPIO ports. There is also a way to provide
a control GPIO for ignoring or decrementing pulses when the control signal is high or low.

[Youtube video of examples using the pulsecnt](https://youtu.be/vk5QZnWdlAA) library including push button counting, stepper motor step counting, and hall effect sensor counting.

## pulsecnt.create()

Create the pulse counter object.

#### Syntax
`pulsecnt.create(unit, callbackOnEvents [, isDebug])`

#### Parameters
- `unit` Required. ESP32 has 0 thru 7 units to count pulses on.
- `callbackOnEvents` Optional. Your Lua method to call on event. myfunction(unit, isThr0, isThr1, isLLim, isHLim, isZero) will be called. Event will be PCNT_EVT_THRES_0 (Threshold 0 hit), PCNT_EVT_THRES_1 (Threshold 1 hit), PCNT_EVT_L_LIM (Minimum counter value), PCNT_EVT_H_LIM (Maximum counter value), or PCNT_EVT_ZERO (counter value zero event)
- `isDebug` Optional. Turn on extra logging by passing in true.

#### Returns
`pulsecntObj` pulsecnt object

#### Example
```lua
-- Example Pulse Counter for 1 channel with polling of pulse count

pinPulseInput  = 2

llim = -32768
hlim = 32767

pcnt = pulsecnt.create(7) -- Use unit 7 (0-7 are allowed)

pcnt:chan0Config(
  pinPulseInput, --pulse_gpio_num
  pulsecnt.PCNT_PIN_NOT_USED, --ctrl_gpio_num If no control is desired specify PCNT_PIN_NOT_USED 
  pulsecnt.PCNT_COUNT_INC, --pos_mode PCNT positive edge count mode
  pulsecnt.PCNT_COUNT_DIS, --neg_mode PCNT negative edge count mode
  pulsecnt.PCNT_MODE_KEEP, --lctrl_mode Ctrl low PCNT_MODE_KEEP, PCNT_MODE_REVERSE, PCNT_MODE_DISABLE
  pulsecnt.PCNT_MODE_KEEP, --hctrl_mode Ctrl high PCNT_MODE_KEEP, PCNT_MODE_REVERSE, PCNT_MODE_DISABLE
  llim, --counter_l_lim [Range -32768 to 32767]
  hlim  --counter_h_lim [Range -32768 to 32767]
)

-- Clear counting
pcnt:clear()

-- Poll the pulse counter
print("Current pulse counter val:" .. pcnt:getCnt())
```

#### Example
```lua
-- Example Pulse Counter for 1 channel with callback

pinPulseInput = 2
thr0 = 2 
thr1 = 4
llim = -6
hlim = 6 

-- Callback on pulsecnt events
function onPulseCnt(unit, isThr0, isThr1, isLLim, isHLim, isZero)

  print("unit:", unit, "isThr0:", isThr0, "isThr1:", isThr1)
  print("isLLim:", isLLim, "isHLim:", isHLim, "isZero:", isZero)
  
  -- Figure out what counter triggered this
  if isThr0 then print("thr0 "..thr0.." triggered event") end 
  if isThr1 then print("thr1 "..thr1.." triggered event") end 
  if isLLim then print("llim "..llim.." triggered event") end 
  if isHLim then print("hlim "..hlim.." triggered event") end 
  if isZero then print("zero 0 triggered event") end 

  -- Get current count
  print("Current pulse counter val:" .. pcnt:getCnt())
end

pcnt = pulsecnt.create(0, onPulseCnt) -- Use unit 0

pcnt:chan0Config(
  pinPulseInput, --pulse_gpio_num
  pulsecnt.PCNT_PIN_NOT_USED, --ctrl_gpio_num If no control is desired specify PCNT_PIN_NOT_USED 
  pulsecnt.PCNT_COUNT_INC, --pos_mode PCNT positive edge count mode PCNT_COUNT_DIS, PCNT_COUNT_INC, PCNT_COUNT_DEC
  pulsecnt.PCNT_COUNT_DIS, --neg_mode PCNT negative edge count mode PCNT_COUNT_DIS, PCNT_COUNT_INC, PCNT_COUNT_DEC
  pulsecnt.PCNT_MODE_KEEP, --lctrl_mode When Ctrl low PCNT_MODE_KEEP, PCNT_MODE_REVERSE, PCNT_MODE_DISABLE
  pulsecnt.PCNT_MODE_KEEP, --hctrl_mode When Ctrl high PCNT_MODE_KEEP, PCNT_MODE_REVERSE, PCNT_MODE_DISABLE
  llim, --counter_l_lim [Range -32768 to 32767]
  hlim  --counter_h_lim [Range -32768 to 32767]
)

-- Clear counting
pcnt:clear()

-- Set counter threshold for low and high
pcnt:setThres(thr0, thr1)

-- Get current pulse counter. Should be zero at start.
print("Current pulse counter val:" .. pcnt:getCnt())
```

#### Example
```lua
-- Example Pulse Counter for 2 push buttons
-- Button 1 increments the counter (chan0)
-- Button 2 decrements the counter (chan1)
-- The pulsecnt filter is used to debounce

pinPulseInput  = 2
pinPulseInput2  = 25

thr0 = 2 
thr1 = 4
llim = -6
hlim = 6 

-- Example callback
function onPulseCnt(unit, isThr0, isThr1, isLLim, isHLim, isZero)
  
  print("unit:", unit, "isThr0:", isThr0, "isThr1:", isThr1)
  print("isLLim:", isLLim, "isHLim:", isHLim, "isZero:", isZero)

  -- Figure out what counter triggered this
  if isThr0 then print("thr0 "..thr0.." triggered event") end 
  if isThr1 then print("thr1 "..thr1.." triggered event") end 
  if isLLim then print("llim "..llim.." triggered event") end 
  if isHLim then print("hlim "..hlim.." triggered event") end 
  if isZero then print("zero 0 triggered event") end 

  -- Get current count
  print("Current pulse counter val:" .. pcnt:getCnt())
  
end

pcnt = pulsecnt.create(0, onPulseCnt) -- Use unit 0

-- Set clock cycles for filter. Any pulses lasting shorter than 
-- this will be ignored when the filter is enabled. So, pulse 
-- needs to be high or low for longer than this filter clock 
-- cycle. Clock is 80Mhz APB clock, so one cycle is 
-- 1000/80,000,000 = 0.0000125 ms. 1023 cycles = 0.0127875 ms
-- so not much of a debounce for manual push buttons.
pcnt:setFilter(1023)

-- Button 1
pcnt:chan0Config(
  pinPulseInput, --pulse_gpio_num
  pulsecnt.PCNT_PIN_NOT_USED, --ctrl_gpio_num If no control is desired specify PCNT_PIN_NOT_USED 
  pulsecnt.PCNT_COUNT_INC, --pos_mode PCNT positive edge count mode
  pulsecnt.PCNT_COUNT_DIS, --neg_mode PCNT negative edge count mode
  pulsecnt.PCNT_MODE_KEEP, --lctrl_mode
  pulsecnt.PCNT_MODE_KEEP, --hctrl_mode 
  llim, --counter_l_lim
  hlim  --counter_h_lim
)

-- Button 2
pcnt:chan1Config(
  pinPulseInput2, --pulse_gpio_num
  pulsecnt.PCNT_PIN_NOT_USED, --ctrl_gpio_num If no control is desired specify PCNT_PIN_NOT_USED 
  pulsecnt.PCNT_COUNT_INC, --pos_mode PCNT positive edge count mode
  pulsecnt.PCNT_COUNT_DIS, --neg_mode PCNT negative edge count mode
  pulsecnt.PCNT_MODE_REVERSE, --lctrl_mode
  pulsecnt.PCNT_MODE_KEEP, --hctrl_mode 
  llim, --counter_l_lim
  hlim  --counter_h_lim
)

-- Clear counting
pcnt:clear()

-- Set counter threshold for low and high
pcnt:setThres(thr0, thr1)

-- Buttons are now setup
```

## pulsecntObj:chan0Config()

Configure channel 0 of the pulse counter object you created from the create() method.

#### Syntax
`pulsecntObj:chan0Config(pulse_gpio_num, ctrl_gpio_num, pos_mode, neg_mode, lctrl_mode, hctrl_mode, counter_l_lim, counter_h_lim)`

#### Parameters
- `pulse_gpio_num` Required. Any GPIO pin can be used.
- `ctrl_gpio_num` Required (although you can specify pulsecnt.PIN_NOT_USED to ignore). Any GPIO pin can be used. If you are trying to use a pin you use as gpio.OUT in other parts of your code, you can instead configure the pin as gpio.IN_OUT and write high or low to it to achieve your gpio.OUT task while still enabling the pulse counter to successfully read the pin state. 
- `pos_mode` Required. Positive rising edge count mode, i.e. count the pulse when the rising edge occurs.
    - pulsecnt.PCNT_COUNT_DIS = 0 Counter mode: Inhibit counter (counter value will not change in this condition). 
    - pulsecnt.PCNT_COUNT_INC = 1 Counter mode: Increase counter value. 
    - pulsecnt.PCNT_COUNT_DEC = 2 Counter mode: Decrease counter value.
- `neg_mode` Required. Negative falling edge count mode, i.e. count the pulse when the falling edge occurs.
    - pulsecnt.PCNT_COUNT_DIS = 0 Counter mode: Inhibit counter (counter value will not change in this condition). 
    - pulsecnt.PCNT_COUNT_INC = 1 Counter mode: Increase counter value. 
    - pulsecnt.PCNT_COUNT_DEC = 2 Counter mode: Decrease counter value.
- `lctrl_mode` Required. When `ctrl_gpio_num` is low how should the counter be influenced.
    - pulsecnt.PCNT_MODE_KEEP = 0 Control mode: will not change counter mode. 
    - pulsecnt.PCNT_MODE_REVERSE = 1 Control mode: invert counter mode (increase -> decrease, decrease -> increase). 
    - pulsecnt.PCNT_MODE_DISABLE = 2 Control mode: Inhibit counter (counter value will not change in this condition).
- `hctrl_mode` Required. When `ctrl_gpio_num` is high how should the counter be influenced.
    - pulsecnt.PCNT_MODE_KEEP = 0 Control mode: will not change counter mode. 
    - pulsecnt.PCNT_MODE_REVERSE = 1 Control mode: invert counter mode (increase -> decrease, decrease -> increase). 
    - pulsecnt.PCNT_MODE_DISABLE = 2 Control mode: Inhibit counter (counter value will not change in this condition).
- `counter_l_lim` Required. Range -32768 to 32767. The lower limit counter. You get a callback at this count and the counter is reset to zero after this lower limit is hit.
- `counter_h_lim` Required. Range -32768 to 32767. The high limit counter. You get a callback at this count and the counter is reset to zero after this high limit is hit.

#### Returns
`nil`

## pulsecntObj:chan1Config()

Configure channel 1 of the pulse counter object you created from the create() method.

#### Syntax
`pulsecntObj:chan1Config(pulse_gpio_num, ctrl_gpio_num, pos_mode, neg_mode, lctrl_mode, hctrl_mode, counter_l_lim, counter_h_lim)`

#### Parameters
- `pulse_gpio_num` Required. Any GPIO pin can be used.
- `ctrl_gpio_num` Required (although you can specify pulsecnt.PIN_NOT_USED to ignore). Any GPIO pin can be used. If you are trying to use a pin you use as gpio.OUT in other parts of your code, you can instead configure the pin as gpio.IN and toggle gpio.PULL_UP or gpio.PULL_DOWN to achieve your gpio.OUT task while still enabling the pulse counter to successfully read the pin state.
- `pos_mode` Required. Positive rising edge count mode, i.e. count the pulse when the rising edge occurs.
    - pulsecnt.PCNT_COUNT_DIS = 0 Counter mode: Inhibit counter (counter value will not change in this condition). 
    - pulsecnt.PCNT_COUNT_INC = 1 Counter mode: Increase counter value. 
    - pulsecnt.PCNT_COUNT_DEC = 2 Counter mode: Decrease counter value.
- `neg_mode` Required. Negative falling edge count mode, i.e. count the pulse when the falling edge occurs.
    - pulsecnt.PCNT_COUNT_DIS = 0 Counter mode: Inhibit counter (counter value will not change in this condition). 
    - pulsecnt.PCNT_COUNT_INC = 1 Counter mode: Increase counter value. 
    - pulsecnt.PCNT_COUNT_DEC = 2 Counter mode: Decrease counter value.
- `lctrl_mode` Required. When `ctrl_gpio_num` is low how should the counter be influenced.
    - pulsecnt.PCNT_MODE_KEEP = 0 Control mode: will not change counter mode. 
    - pulsecnt.PCNT_MODE_REVERSE = 1 Control mode: invert counter mode (increase -> decrease, decrease -> increase). 
    - pulsecnt.PCNT_MODE_DISABLE = 2 Control mode: Inhibit counter (counter value will not change in this condition).
- `hctrl_mode` Required. When `ctrl_gpio_num` is high how should the counter be influenced.
    - pulsecnt.PCNT_MODE_KEEP = 0 Control mode: will not change counter mode. 
    - pulsecnt.PCNT_MODE_REVERSE = 1 Control mode: invert counter mode (increase -> decrease, decrease -> increase). 
    - pulsecnt.PCNT_MODE_DISABLE = 2 Control mode: Inhibit counter (counter value will not change in this condition).
- `counter_l_lim` Required. Range -32768 to 32767. The lower limit counter. You get a callback at this count and the counter is reset to zero after this lower limit is hit.
- `counter_h_lim` Required. Range -32768 to 32767. The high limit counter. You get a callback at this count and the counter is reset to zero after this high limit is hit.

#### Returns
`nil`

#### Example
```lua
-- Button 1 increment counter
pcnt:chan0Config(
  pinPulseInput, --pulse_gpio_num
  pulsecnt.PCNT_PIN_NOT_USED, --ctrl_gpio_num If no control is desired specify PCNT_PIN_NOT_USED 
  pulsecnt.PCNT_COUNT_INC, --pos_mode PCNT positive edge count mode
  pulsecnt.PCNT_COUNT_DIS, --neg_mode PCNT negative edge count mode
  pulsecnt.PCNT_MODE_KEEP, --lctrl_mode
  pulsecnt.PCNT_MODE_KEEP, --hctrl_mode 
  llim, --counter_l_lim
  hlim  --counter_h_lim
)

-- Button 2 decrement counter
pcnt:chan1Config(
  pinPulseInput2, --pulse_gpio_num
  pulsecnt.PCNT_PIN_NOT_USED, --ctrl_gpio_num If no control is desired specify PCNT_PIN_NOT_USED 
  pulsecnt.PCNT_COUNT_INC, --pos_mode PCNT positive edge count mode
  pulsecnt.PCNT_COUNT_DIS, --neg_mode PCNT negative edge count mode
  pulsecnt.PCNT_MODE_REVERSE, --lctrl_mode
  pulsecnt.PCNT_MODE_KEEP, --hctrl_mode 
  llim, --counter_l_lim
  hlim  --counter_h_lim
)
```

## pulsecntObj:setThres()

Set the threshold values to establish watchpoints for getting callbacks on.

#### Syntax
`pulsecntObj:setThres(thr0, thr1)`

#### Parameters
- `thr0` Required. Threshold 0 value. Range -32768 to 32767. This is a watchpoint value to get a callback with isThr0 set to true on this count being reached. 
- `thr1` Required. Threshold 1 value. Range -32768 to 32767. This is a watchpoint value to get a callback with isThr1 set to true on this count being reached.

#### Returns
`nil`

#### Example
```lua
pcnt:setThres(-2000, 2000) -- get callbacks when counter is -2000 or 2000
```
```lua
pcnt:setThres(500, 10000) -- get callbacks when counter is 500 or 10000
```

## pulsecntObj:setFilter()

Set a filter to ignore pulses on the `pulse_gpio_num` pin and the `ctrl_gpio_num` to avoid short glitches. Any pulses lasting shorter than this will be ignored.

#### Syntax
`pulsecntObj:setFilter(clkCycleCnt)`

#### Parameters
- `clkCycleCnt` Required. 0 to 1023 allowd. Any pulses lasting shorter than this will be ignored. A pulse needs to be high or low for longer than this filter clock cycle. Clock is 80Mhz APB clock, so one cycle is 1000/80,000,000 = 0.0000125 ms. The longest value of 1023 cycles = 0.0127875 ms.

#### Returns
`nil`

#### Example
```lua
pcnt:setFilter(1023) -- set max filter clock cylce count to ignore pulses shorter than 12.7us
```

## pulsecntObj:clear()

Clear the counter. Sets it back to zero.

#### Syntax
`pulsecntObj:clear()`

#### Parameters
None

#### Returns
`nil`

#### Example
```lua
pcnt:clear() -- set counter back to zero
```

## pulsecntObj:getCnt()

Get the current pulse counter.

#### Syntax
`pulsecntObj:getCnt()`

#### Parameters
None

#### Returns
`integer`

#### Example
```lua
val = pcnt:getCnt() -- get counter
print("Pulse counter:", val)
```

