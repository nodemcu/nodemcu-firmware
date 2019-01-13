# DHT Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-03-30 | [Arnim Läuger](https://github.com/devsaurus) | [Arnim Läuger](https://github.com/devsaurus) | [dht](../../components/modules/dht.c)|

## Constants
Constants for various functions.

`dht.OK`, `dht.ERROR_CHECKSUM`, `dht.ERROR_TIMEOUT` represent the potential values for the DHT read status

## dht.read11()
Read DHT11 humidity temperature combo sensor.

#### Syntax
`dht.read11(pin)`

#### Parameters
`pin` IO index, see [GPIO Overview](gpio.md#gpio-overview)

#### Returns
- `status` as defined in Constants
- `temp` temperature
- `humi` humidity
- `temp_dec` temperature decimal (always 0)
- `humi_dec` humidity decimal (always 0)

#### Example
```lua
pin = 4
status, temp, humi = dht.read11(pin)
if status == dht.OK then
    print("DHT Temperature:"..temp..";".."Humidity:"..humi)

elseif status == dht.ERROR_CHECKSUM then
    print( "DHT Checksum error." )
elseif status == dht.ERROR_TIMEOUT then
    print( "DHT timed out." )
end
```

## dht.read2x()
Read DHT21/22/33/43 and AM2301/2302/2303 humidity temperature combo sensors.

#### Syntax
`dht.read2x(pin)`

#### Parameters
`pin` IO index, see [GPIO Overview](gpio.md#gpio-overview)

#### Returns
- `status` as defined in Constants
- `temp` temperature (see note below)
- `humi` humidity (see note below)
- `temp_dec` temperature decimal
- `humi_dec` humidity decimal

!!! note

    If using float firmware then `temp` and `humi` are floating point numbers. On an integer firmware, the final values have to be concatenated from `temp` and `temp_dec` / `humi` and `hum_dec`.

#### Example
```lua
pin = 4
status, temp, humi, temp_dec, humi_dec = dht.read2x(pin)
if status == dht.OK then
    -- Integer firmware using this example
    print(string.format("DHT Temperature:%d.%03d;Humidity:%d.%03d\r\n",
          math.floor(temp),
          temp_dec,
          math.floor(humi),
          humi_dec
    ))

    -- Float firmware using this example
    print("DHT Temperature:"..temp..";".."Humidity:"..humi)

elseif status == dht.ERROR_CHECKSUM then
    print( "DHT Checksum error." )
elseif status == dht.ERROR_TIMEOUT then
    print( "DHT timed out." )
end
```
