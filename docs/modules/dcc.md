# DCC module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-12-28 | [vsky279](https://github.com/vsky279) | [vsky279](https://github.com/vsky279) | [dcc.c](../../app/modules/dcc.c)|

The dcc module implements decoder of the [National Model Railroad Association](https://www.nmra.org/) (NMRA) Digital Command Control (DCC) decoder - see [DCC wiki](https://dccwiki.com/Introduction_to_DCC) for details.

The hardware needed to decode the DCC signal can be built based on different DCC decoders implementation for Arduino, for inspiration see [https://mrrwa.org/dcc-decoder-interface/](https://mrrwa.org/dcc-decoder-interface/). Basically the signal from the DCC bus is connected via an optocoupler to any GPIO pin. The DCC bus can be also used to power the ESP.

The module is based on the project NmraDcc [https://github.com/mrrwa/NmraDcc](https://github.com/mrrwa/NmraDcc) by Alex Shepherd (@kiwi64ajs). The module is based on latest commit from Oct 2020, commit [7e3b3e346991d74926e6c4f6fb46e27156b08578](https://github.com/mrrwa/NmraDcc/tree/7e3b3e346991d74926e6c4f6fb46e27156b08578).

## dcc.setup()

Initializes the dcc module and links callback functions.

#### Syntax
`dcc.setup(Pin, [AckPin, ] DCC_command, ManufacturerId, VersionId, Flags, OpsModeAddressBaseCV [, CV_table] [, CV_callback])`

#### Parameters
- `Pin` the GPIO pin number connected to the DCC detector (must be interrupt capable pin).
- `AckPin` the (optional) GPIO pin number connected to the ACK mechanism. Will be set HIGH to signal an ACK.
- `DCC_command(cmd, params)` calllback function that is called when a DCC command is decoded. `cmd` parameters is one of the following values. `params` contains a collection of parameters specific to given command.
    -  `dcc.DCC_RESET` no additional parameters, `params` is `nil`.
    -  `dcc.DCC_IDLE` no additional parameters, `params` is `nil`.
    -  `dcc.DCC_SPEED` parameters collection members are `Addr`, `AddrType`, `Speed`, `Dir`, `SpeedSteps`.
    -  `dcc.DCC_SPEED_RAW`  parameters collection members are `Addr`, `AddrType`, `Raw`.
    -  `dcc.DCC_FUNC`  parameters collection members are  `Addr`, `AddrType`, `FuncGrp`, `FuncState`.
    -  `dcc.DCC_TURNOUT` parameters collection members are `BoardAddr`, `OutputPair`, `Direction`, `OutputPower` or `Addr`, `Direction`, `OutputPower`.
    -  `dcc.DCC_ACCESSORY` parameters collection has one member `BoardAddr` or `Addr` or `State`.
    -  `dcc.DCC_RAW` parameters collection member are `Size`, `PreambleBits`, `Data1` to `Data6`.
    -  `dcc.DCC_SERVICEMODE`  parameters collection has one member `InServiceMode`.
- `ManufacturerId` Manufacturer ID returned in CV 8. Commonly `dcc.MAN_ID_DIY`.
- `VersionId` Version ID returned in CV 7.
- `Flags` one of or combination (OR operator) of 
    - `dcc.FLAGS_MY_ADDRESS_ONLY`Only process packets with My Address.
    - `dcc.FLAGS_DCC_ACCESSORY_DECODER` Decoder is an accessory decode.
    - `dcc.FLAGS_OUTPUT_ADDRESS_MODE` This flag applies to accessory decoders only. Accessory decoders normally have 4 paired outputs and a single address refers to all 4 outputs. Setting this flag causes each address to refer to a single output.
    - `dcc.FLAGS_AUTO_FACTORY_DEFAULT`  Call DCC command callback with `dcc.CV_RESET` command if CV 7 & 8 == 255.
- `OpsModeAddressBaseCV`  Ops Mode base address. Set it to 0?
- `CV_table`  The CV values will be directly accessed from this table. metamethods will be invoked if needed. Any errors thrown will cause the CV to be considered invalid. Using this option will prevent `CV_VALID`, `CV_READ`, `CV_WRITE` and `CV_ACK_COMPLETE` from happening.
- `CV_callback(operation, param)` callback function that is called when any manipulation with CV ([Configuarion Variable](https://dccwiki.com/Configuration_Variable)) is requested.
    -  `dcc.CV_VALID` to determine if a given CV is valid and (possibly) writable. This callback must determine if a CV is readable or writable and return the appropriate value(0/1/true/false). The `param` collection has members `CV` and `Writable`.
    -  `dcc.CV_READ` to read a CV. This callback must return the value of the CV. The `param` collection has one member `CV` determing the CV number to be read.
    -  `dcc.CV_WRITE` to write a value to a CV. This callback must write the Value to the CV and return the value of the CV. The `param` collection has members `CV` and `Value`. Ideally, the final value should be returned -- this may differ from the requested value.
    -  `dcc.CV_RESET` Called when CVs must be reset to their factory defaults. 
    -  `dcc.CV_ACK_COMPLETE` Called when an ACK pulse has finished being sent. Only invoked if `AckPin` is specified.

#### Returns
`nil`

#### Example
`bit` module is used in the example though it is not needed for the dcc module functionality.
```lua
local PIN = 2 -- GPIO4

local addr = 0x12a

CV = {[29]=0, 
      [1]=bit.band(addr, 0x3f), --CV_ACCESSORY_DECODER_ADDRESS_LSB (6 bits)
      [9]=bit.band(bit.rshift(addr,6), 0x7)  --CV_ACCESSORY_DECODER_ADDRESS_MSB (3 bits)
     }

local function DCC_command(cmd, params)
    if cmd == dcc.DCC_IDLE then 
        return
    elseif cmd == dcc.DCC_TURNOUT then
        print("Turnout command") 
    elseif cmd == dcc.DCC_SPEED then
        print("Speed command") 
    elseif cmd == dcc.DCC_FUNC then
        print("Function command") 
    else
        print("Other command", cmd)
    end
    
    for i,j in pairs(params) do
        print(i, j)
    end
    print(("="):rep(80))
end

local function CV_callback(operation, param)
    local oper = ""
    local result
    if operation == dcc.CV_WRITE then
        oper = "Write"
        CV[param.CV]=param.Value
    elseif operation == dcc.CV_READ then
        oper = "Read"
        result = CV[param.CV]
    elseif operation == dcc.CV_VALID then
        oper = "Valid"
        result = 1
    elseif operation == CV_RESET then
        oper = "Reset"
        CV = {}
    end
    print(("[CV_callback] %s CV %d%s"):format(oper, param.CV or `nil`, param.Value and "\tValue: "..param.Value or "\tValue: nil"))
    return result
end

dcc.setup(PIN,
    DCC_command,
    dcc.MAN_ID_DIY, 1, 
    --bit.bor(dcc.FLAGS_AUTO_FACTORY_DEFAULT, dcc.FLAGS_DCC_ACCESSORY_DECODER, dcc.FLAGS_MY_ADDRESS_ONLY), 
    bit.bor(dcc.FLAGS_AUTO_FACTORY_DEFAULT), 
    0, -- ???
    CV_callback)
```

## dcc.close()

Stops the dcc module.

#### Syntax
`dcc.close()`

#### Parameters
`nil`

#### Returns
`nil`
