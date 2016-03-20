# encoder Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-02-26 | [Terry Ellison](https://github.com/TerryE) | [Terry Ellison](https://github.com/TerryE) | [encoder.c](../../../app/modules/encoder.c)|

The encoder modules provides various functions for encoding and decoding byte data.

## encoder.toBase64()

Provides a Base64 representation of a (binary) Lua string.

#### Syntax
`b64 = encoder.toBase64(binary)`

#### Parameters
`binary` input string to Base64 encode

#### Return
A Base64 encoded string.

#### Example
```lua
print(encoder.toBase64(crypto.hash("sha1","abc")))
```

## encoder.fromBase64()

Decodes a Base64 representation of a (binary) Lua string back into the original string.  An error is
thrown if the string is not a valid base64 encoding.

#### Syntax
`binary_string = encoder.toBase64(b64)`

#### Parameters
`b64` Base64 encoded input string 

#### Return
The decoded Lua (binary) string.

#### Example
```lua
print(encoder.fromBase64(encoder.toBase64("hello world")))
```

## encoder.toHex()

Provides an ASCII hex representation of a (binary) Lua string. Each byte in the input string is
represented as two hex characters in the output.

#### Syntax
`hexstr = encoder.toHex(binary)`

#### Parameters
`binary` input string to get hex representation for

#### Returns
An ASCII hex string.

#### Example
```lua
print(encoder.toHex(crypto.hash("sha1","abc")))
```

## encoder.fromHex()

Returns the Lua binary string decode of a ASCII hex string. Each byte in the output string is
represented as two hex characters in the input.  An error is thrown if the string is not a 
valid base64 encoding.

#### Syntax
`binary = encoder.fromHex(hexstr)`

#### Parameters
`hexstr`  An ASCII hex string.

#### Returns
Decoded string of hex representation.

#### Example
```lua
print(encoder.fromHex("6a6a6a")))
```
