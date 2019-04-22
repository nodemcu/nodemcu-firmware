# Pulse Counter Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-04-07 | [ChiliPeppr](https://github.com/chilipeppr) | John Lauer | [pulsecnt.c](../../components/modules/pulsecnt.c)|

pulsecnt is a library to handle pulse counting using the built-in hardware on ESP32.

## pulsecnt.create()

Create the pulse counter object.

### Syntax
`pulsecnt.create(unit, callbackOnEvents)`

### Parameters
- `unit` Defaults to 0. ESP32 has 0 thru 7 units to count pulses on.
- `callbackOnEvents` Your Lua method to call on event. myfunction(unit, event, threshold) will be called. Event will be PCNT_EVT_L_LIM (Minimum counter value), PCNT_EVT_H_LIM (Maximum counter value), or PCNT_EVT_ZERO (counter value zero event)

### Returns
`pulsecnt` object

### Example
```lua
-- Example for keeping a step pulse counter for a stepper motor 
-- with step pin on gpio 2 and direction pin on gpio 4. 
-- Count steps on rising positive edge of pin 2.
-- Decrement steps when direction pin is high (meaning stepper motor direction is reversed).
function onPulseCnt(unit, event, threshold)
  print("Got pulse counter callback. unit", unit, "event:", event)
end

pc = pulsecnt.create(0, onPulseCnt)
pc:channel0Config(2, 4, pulsecnt.PCNT_MODE_KEEP, pulsecnt.PCNT_MODE_REVERSE)

-- pc = pulsecnt.create({
--   unit = 0,
--   channel0 = {gpioPulseInput = 2, gpioCtrlInput = 4, ctrlModeLow = pulsecnt.PCNT_MODE_KEEP, ctrlModeHigh = pulsecnt.PCNT_MODE_REVERSE},
--   callback = onPulseCnt,
--   })

-- Move stepper 1200 steps by turning on PWM and listening to the pulses on pin 2
-- The moment we get the callback from our 1200 steps set using setThres() we turn
-- off PWM to stop the stepper motor from moving
pc:channel0SetThres(1200) -- Set threshold to get counter callback after 1200 pulses

-- Get current pulse counter
print("Current pulse counter val:" .. pc:getCnt())

-- Clear counting
pc:clear() -- Set counter to zero

```

#### Syntax
```lua
pulsecnt.create({
  unit = 0, -- Defaults to 0. Use this value for all subsequent methods. ESP32 has 0 thru 7 units to count pulses on
  channel0 = {
    gpioPulseInput = 12, -- REQUIRED. GPIO for incoming pulses to be counted.
    gpioCtrlInput = 14, -- REQUIRED. GPIO for control input. See ctrlModeLow and ctrlModeHigh for what to do when control input is low or high. For example, this is like a direction pin on a stepper motor. If direction is high or low, you would count up or down on the pulse counter.
    ctrlModeLow = pulse.PCNT_MODE_KEEP, -- Defaults to pulse.PCNT_MODE_KEEP (won’t change counter mode). What to do when gpioCtrlInput pin is low. Other options pulse.PCNT_MODE_REVERSE (invert counter mode. increase -> decrease, decrease -> increase), or pulse.PCNT_MODE_DISABLE (inhibit counter. counter value will not change in this condition)
    ctrlModeHigh = pulse.PCNT_MODE_DISABLE, -- Same as ctrlModeLow options, but for what happens when gpioCtrlInput pin is high
    countModePosEdge = pulse.PCNT_COUNT_INC, -- Default to pulse.PCNT_COUNT_INC (increase counter). Other options pulse.PCNT_COUNT_DEC (decrease counter) and pulse.PCNT_COUNT_DIS (inhibit counting)
    countModeNegEdge = pulse.PCNT_COUNT_DIS, -- Same as countModePosEdge, but for negative edge
    thres=1000, -- Defaults to 32767. Range int16 [-32768 : 32767]. Get callback when channel 0 gets a counter at or above this threshold. Change later using pulsecnt.callbackSetEventVal(unit, pulse.PCNT_EVT_THRES_0, value)
  },
  channel1 = {
    -- same as above
  },
    -- the unit-related items
  ctrHighLimit = 10, -- Defaults to 32767. Range int16 [-32768 : 32767]. Maximum counter value. Change later using pulsecnt.callbackSetEventVal(unit, pulse.PCNT_EVT_H_LIM, value)
  ctrLowLimit = -10, -- Defaults to -32767. Range int16 [-32768 : 32767]. Minimum counter value. Change later using pulsecnt.callbackSetEventVal(unit, pulse.PCNT_EVT_L_LIM, value)
})
```

