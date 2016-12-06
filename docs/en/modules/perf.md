# perf Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-02-26 | [Philip Gladstone](https://github.com/pjsg) | [Philip Gladstone](https://github.com/pjsg) | [perf.c](../../../app/modules/perf.c)|


This module provides simple performance measurement for an application. It samples the program counter roughly every 50 microseconds and builds a histogram of the values that it finds. Since there is only a small amount
of memory to store the histogram, the user can specify which area of code is of interest. The default is the entire flash which contains code. Once the hotspots are identified, then the run can then be repeated with different areas and at different resolutions to get as much information as required.

## perf.start()
Starts a performance monitoring session. 

#### Syntax
`perf.start([start[, end[, nbins]]])`

#### Parameters
- `start` (optional) The lowest PC address for the histogram. Default is 0x40000000.
- `end` (optional) The highest address for the histogram. Default is the end of the used space in the flash memory.
- `nbins` (optional) The number of bins in the histogram. Keep this reasonable otherwise 
you will run out of memory. Default is 1024.

Note that the number of bins is an upper limit. The size of each bin is set to be the smallest power of two
such that the number of bins required is less than or equal to the provided number of bins.

#### Returns
Nothing

## perf.stop()

Terminates a performance monitoring session and returns the histogram.

#### Syntax
`total, outside, histogram, binsize = perf.stop()`

#### Returns
- `total` The total number of samples captured in this run
- `outside` The number of samples that were outside the histogram range
- `histogram` The histogram represented as a table indexed by address where the value is the number of samples. The address is the lowest address for the bin.
- `binsize` The number of bytes per histogram bin.

### Example

    perf.start()

    for j = 0, 100 do
      str = "str"..j
    end

    tot, out, tbl, binsize = perf.stop()

    print(tot, out)
    local keyset = {}
    local n = 0
    for k,v in pairs(tbl) do
      n=n+1
      keyset[n]=k
    end
    table.sort(keyset)
    for kk,k in ipairs(keyset) do print(string.format("%x - %x",k, k + binsize - 1),tbl[k]) end

This runs a loop creating strings 100 times and then prints out the histogram (after sorting it).
This takes around 2,500 samples and provides a good indication of where all the CPU time is
being spent. 
