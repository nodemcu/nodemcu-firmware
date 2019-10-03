# Benchmark Module

| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-05-24 | [fikin](https://github.com/fikin) | [fikin](https://github.com/fikin) | [benchmark.c](../../../app/modules/benchmark.c)|

Module to benchmark performance of various NodeMCU and ESP8266 operations.

All benchmarking internally is done using CPU ticks. 

By default every test is repeated 2000 times before its duration is calculated. But for CPU160 one would probably want to increase it in order to obtain more precise results.

This module is thought to be useful mainly to other modules developers to understand better low level API timing aspects. And to provide with basis for implmenenting other custom benchmarking metrics.

Supported are CPU80 and CPU160.

Typical usage is as following:

```lua
print(benchmark.empty_call())
print(benchmark.single_nop())
print(benchmark.get_cpu_counts())
print(benchmark.re_recurse())
print(benchmark.us1_sleep())
print(benchmark.us10_sleep())
print(benchmark.us100_sleep())
print(benchmark.gpio_read_pin())
print(benchmark.gpio_pull_updown())
print(benchmark.gpio_status_read())
print(benchmark.gpio_status_write())
print(benchmark.adc_read())
print(benchmark.system_get_time())
print(benchmark.system_get_rtc_time())
print(benchmark.system_get_cpufreq())
print(benchmark.memcpy_1k_bytes())
print(benchmark.memset_1k_bytes())
print(benchmark.bzero_1k_bytes())
print(benchmark.gpio_set_one(4))
benchmark.frc1_manual_exclusive(10, false,
    function(osOverhead, testingHandlerOverhed, totalTestTime)
        print(osOverhead, testingHandlerOverhed, totalTestTime)
    end)
benchmark.frc1_autoload_exclusive(10, false,
    function(osOverhead, testingHandlerOverhed, totalTestTime)
        print(osOverhead, testingHandlerOverhed, totalTestTime)
    end)
benchmark.frc1_manual_shared(10, false,
    function(osOverhead, testingHandlerOverhed, totalTestTime)
        print(osOverhead, testingHandlerOverhed, totalTestTime)
    end)
benchmark.frc1_autoload_shared(10, false,
    function(osOverhead, testingHandlerOverhed, totalTestTime)
        print(osOverhead, testingHandlerOverhed, totalTestTime)
    end
```

Credits to [https://sub.nanona.fi/esp8266/timing-and-ticks.html](https://sub.nanona.fi/esp8266/timing-and-ticks.html) for inspiring some of the tests and setting up the standard for the rest.

## benchmark.ccount()

Get value of CPU CCOUNT register which contains CPU ticks. It supports CPU80 and CPU160.

This allows for calculation of elapsed time with microsecond precision. For example for CPU80 there are 80 ticks/us (80000 ticks/ms).

Note the register is 32-bits and rolls over.

### Syntax

`benchmark.ccount()`

### Returns

The current value of CCOUNT register.

### Example

```lua
print ("benchmark.ccount() takes ", benchmark.ccount()-benchmark.ccount(), " CPU ticks to execute.")
```

## benchmark.bench_lua_func()

Benchmark the given lua function.

### Syntax

`benchmark.bench_lua_func(function() end)`

### Returns

The time to execute the function in terms of CPU ticks.

### Example

```lua
print ("empty lua function takes ", benchmark.bench_lua_func(function()end), " CPU ticks to execute.")
```

## benchmark.set_repetitions()

Assign new repetitions value. By default the module uses 2000.

### Syntax

`benchmark.set_repetitions(repetitionsCnt)`

### Parameters

- `repetitionsCnt` int, new repetitions count to be used by benchmarking tests.

### Returns

`nil`

### See also

- [benchmark.get_repetitions()](#benchmarkget_repetitions)

## benchmark.get_repetitions()

Get number of repetitions each test is repeated before its time is benchmarked.

### Syntax

`benchmark.get_repetitions()`

### Parameters

`nil`

### Returns

- `repetitionsCnt` int, repetitions count to be used by benchmarking tests.

### See also

- [benchmark.set_repetitions()](#benchmarkset_repetitions)

## benchmark.empty_call()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.empty_call()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.single_nop()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.single_nop()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.get_cpu_counts()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.get_cpu_counts()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.re_recurse()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.re_recurse()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.us1_sleep()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.us1_sleep()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.us10_sleep()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.us10_sleep()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.us100_sleep()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.us100_sleep()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.gpio_read_pin()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.gpio_read_pin()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.gpio_pull_updown()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).
Test is using GPIO_OUTPUT_SET to pull pin 5 up and then again to pull it down.

### Syntax

`benchmark.gpio_pull_updown()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.gpio_status_read()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.gpio_status_read()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.gpio_status_write()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.gpio_status_write()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.adc_read()

Times reading of analog input (ADC) using `system_adc_read()` function.

### Syntax

`benchmark.adc_read()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.system_get_time()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.system_get_time()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.system_get_rtc_time()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.system_get_rtc_time()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.system_get_cpufreq()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.system_get_cpufreq()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.memcpy_1k_bytes()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.memcpy_1k_bytes()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.memset_1k_bytes()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.memset_1k_bytes()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.bzero_1k_bytes()

See [external documentation](https://sub.nanona.fi/esp8266/timing-and-ticks.html).

### Syntax

`benchmark.bzero_1k_bytes()`

### Parameters

`nil`

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.gpio_set_one()

Pulls given pin up using `GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pinAsMask)`.
This is perhaps the fastest gpio operation from all available.

### Syntax

`benchmark.gpio_set_one(pin)`

### Parameters

- `pin` 1~12, IO index

### Returns

- `cpuTicks` int, CPU ticks needed to perform the operation

## benchmark.frc1_manual_exclusive()

Test the operating system overhead when dispatching TIMER1 FRC1 events using MANUAL mode.

Timer is used in exclusive mode i.e. no other module can use it at the same time.

The test handler is coded to start measurement after first interrupt.

This test uses as input as timer ticks which equals to 16 CPU ticks for CPU80 and 32 CPU ticks for CPU160.

Times measured is interrupt switching time inside operating system, test's own handler time and total test duration.

Operating system time is also including the waiting time between interrupts. If one specifies 0 timer ticks this delay is none. But there is a good change the board would brick.

This method allows for recording CPU ticks of each interrupt call. Recording adds a little overhead to internal timer handler and allocates memory for all repetitions. The recorded values can be read via `benchmark.get_recorded_interrupt_counter(repetitionNbr)`. These values allow for further analysis of timing precisions and quality.

### Syntax

`benchmark.frc1_manual_exclusive(timerTicks,gatherDelays,callback_function)`

### Parameters

- `timerTicks` int, timer ticks to load TIMER1 FRC1 each time with.
- `gatherDelays` bool, should it gather current CPU ticks counter of each individual interrupt.
- `callback_function(osOverhead,testHandlerOwnTime,testDuration)` callback function when timer test ends.
    - `osOverhead` int, CPU ticks of operating system overhead plus delay needed to fulfil timerTicks interrupt frequency.
    - `testHandlerOwnTime` int, CPU ticks of test's own handler overhead
    - `testDuration` int, total test duration as CPU ticks.

### Returns

`nil`

### See also

- [benchmark.get_recorded_interrupt_counter()](#benchmarkget_recorded_interrupt_counter)

## benchmark.frc1_autoload_exclusive()

Test the operating system overhead when dispatching TIMER1 FRC1 events using AUTO mode.

Timer is used in exclusive mode i.e. no other module can use it at the same time.

The test handler is coded to start measurement after first interrupt.

This test uses as input as timer ticks which equals to 16 CPU ticks for CPU80 and 32 CPU ticks for CPU160.

Times measured is interrupt switching time inside operating system, test's own handler time and total test duration.

Operating system time is also including the waiting time between interrupts. If one specifies 0 timer ticks this delay is none. But there is a good change the board would brick.

This method allows for recording CPU ticks of each interrupt call. Recording adds a little overhead to internal timer handler and allocates memory for all repetitions. The recorded values can be read via `benchmark.get_recorded_interrupt_counter(repetitionNbr)`. These values allow for further analysis of timing precisions and quality.

### Syntax

`benchmark.frc1_autoload_exclusive(timerTicks,gatherDelays,callback_function)`

### Parameters

- `timerTicks` int, timer ticks to load TIMER1 FRC1 each time with.
- `gatherDelays` bool, should it gather current CPU ticks counter of each individual interrupt.
- `callback_function(osOverhead,testHandlerOwnTime,testDuration)` callback function when timer test ends.
    - `osOverhead` int, CPU ticks of operating system overhead plus delay needed to fulfil timerTicks interrupt frequency.
    - `testHandlerOwnTime` int, CPU ticks of test's own handler overhead
    - `testDuration` int, total test duration as CPU ticks.

### Returns

`nil`

### See also

- [benchmark.get_recorded_interrupt_counter()](#benchmarkget_recorded_interrupt_counter)

## benchmark.frc1_manual_shared()

Test the operating system overhead when dispatching TIMER1 FRC1 events using MANUAL mode.

Timer is used in shared mode i.e. other modules can use it at the same time.

The test handler is coded to start measurement after first interrupt.

This test uses as input as timer ticks which equals to 16 CPU ticks for CPU80 and 32 CPU ticks for CPU160.

Times measured is interrupt switching time inside operating system, test's own handler time and total test duration.

Operating system time is also including the waiting time between interrupts. If one specifies 0 timer ticks this delay is none. But there is a good change the board would brick.

This method allows for recording CPU ticks of each interrupt call. Recording adds a little overhead to internal timer handler and allocates memory for all repetitions. The recorded values can be read via `benchmark.get_recorded_interrupt_counter(repetitionNbr)`. These values allow for further analysis of timing precisions and quality.

### Syntax

`benchmark.frc1_manual_shared(timerTicks,gatherDelays,callback_function)`

### Parameters

- `timerTicks` int, timer ticks to load TIMER1 FRC1 each time with.
- `gatherDelays` bool, should it gather current CPU ticks counter of each individual interrupt.
- `callback_function(osOverhead,testHandlerOwnTime,testDuration)` callback function when timer test ends.
    - `osOverhead` int, CPU ticks of operating system overhead plus delay needed to fulfil timerTicks interrupt frequency.
    - `testHandlerOwnTime` int, CPU ticks of test's own handler overhead
    - `testDuration` int, total test duration as CPU ticks.

### Returns

`nil`

### See also

- [benchmark.get_recorded_interrupt_counter()](#benchmarkget_recorded_interrupt_counter)

## benchmark.frc1_autoload_shared()

Test the operating system overhead when dispatching TIMER1 FRC1 events using AUTO mode.

Timer is used in shared mode i.e. other modules can use it at the same time.

The test handler is coded to start measurement after first interrupt.

This test uses as input as timer ticks which equals to 16 CPU ticks for CPU80 and 32 CPU ticks for CPU160.

Times measured is interrupt switching time inside operating system, test's own handler time and total test duration.

Operating system time is also including the waiting time between interrupts. If one specifies 0 timer ticks this delay is none. But there is a good change the board would brick.

This method allows for recording CPU ticks of each interrupt call. Recording adds a little overhead to internal timer handler and allocates memory for all repetitions. The recorded values can be read via `benchmark.get_recorded_interrupt_counter(repetitionNbr)`. These values allow for further analysis of timing precisions and quality.

### Syntax

`benchmark.frc1_autoload_shared(timerTicks,gatherDelays,callback_function)`

### Parameters

- `timerTicks` int, timer ticks to load TIMER1 FRC1 each time with.
- `gatherDelays` bool, should it gather current CPU ticks counter of each individual interrupt.
- `callback_function(osOverhead,testHandlerOwnTime,testDuration)` callback function when timer test ends.
    - `osOverhead` int, CPU ticks of operating system overhead plus delay needed to fulfil timerTicks interrupt frequency.
    - `testHandlerOwnTime` int, CPU ticks of test's own handler overhead
    - `testDuration` int, total test duration as CPU ticks.

### Returns

`nil`

### See also

- [benchmark.get_recorded_interrupt_counter()](#benchmarkget_recorded_interrupt_counter)

## benchmark.get_recorded_interrupt_counter()

Get recorded in previous run CPU ticks counter at the moment of timer interrupt. Methods testing timer interrupts are `benchmark.frc1_XXX_XXX()`. If one runs them with `gatherDelays = True` this method will provide access to the records.

### Syntax

`benchmark.get_recorded_interrupt_counter(repetitionNbr)`

### Parameters

- `repetitionNbr` 1-repetitions, CPU ticks counter of which repetitions to return.

### Returns

- `cpuTicksCounter` int, CPU ticks counter or 0 if there is not such recording

### Example

```lua
-- run some interrupt test and record the CPU counter at each interrupt
print(benchmark.set_repetitions(50))
benchmark.frc1_manual_exclusive(10, true,
    function(osOverhead, testingHandlerOverhed, totalTestTime)
        print(osOverhead, testingHandlerOverhed, totalTestTime)
        for i=1,benchmark.get_repetitions() do
            print("CPU counter at interrupt "..i.." = "..benchmark.get_recorded_interrupt_counter(i))
        end
    end)

```
