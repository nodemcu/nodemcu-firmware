# DHT Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-06-17 | [RobTillaart](https://github.com/RobTillaart/Arduino/tree/master/libraries/DHTlib) | [Vowstar](https://github.com/vowstar) | [dhtlib](../../../app/dhtlib/)|

## Constants
Constants for various functions.

`dht.OK`, `dht.ERROR_CHECKSUM`, `dht.ERROR_TIMEOUT` represent the potential values for the DHT read status

## dht.read()
Read all kinds of DHT sensors, including DHT11, 21, 22, 33, 44 humidity temperature combo sensor.

#### Syntax
`dht.read(pin)`

#### Parameters
`pin` pin number of DHT sensor (can't be 0), type is number

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
pin = 5
status, temp, humi, temp_dec, humi_dec = dht.read(pin)
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

## dht.read11()
Read DHT11 humidity temperature combo sensor.

#### Syntax
`dht.read11(pin)`

#### Parameters
`pin` pin number of DHT11 sensor (can't be 0), type is number

#### Returns
- `status` as defined in Constants
- `temp` temperature (see note below)
- `humi` humidity (see note below)
- `temp_dec` temperature decimal
- `humi_dec` humidity decimal

!!! note

    If using float firmware then `temp` and `humi` are floating point numbers. On an integer firmware, the final values have to be concatenated from `temp` and `temp_dec` / `humi` and `hum_dec`.

#### See also
[dht.read()](#dhtread)

## dht.readxx()
Read all kinds of DHT sensors, except DHT11.

####Syntax
`dht.readxx(pin)`

#### Parameters
`pin` pin number of DHT sensor (can't be 0), type is number

#### Returns
- `status` as defined in Constants
- `temp` temperature (see note below)
- `humi` humidity (see note below)
- `temp_dec` temperature decimal
- `humi_dec` humidity decimal

!!! note

    If using float firmware then `temp` and `humi` are floating point numbers. On an integer firmware, the final values have to be concatenated from `temp` and `temp_dec` / `humi` and `hum_dec`.

#### See also
[dht.read()](#dhtread)
