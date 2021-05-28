# QR Code Generator Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2018-10-20 | https://github.com/nayuki/QR-Code-generator.git integrated by [Tom Sutcliffe](https://github.com/tomsci) | [Tom Sutcliffe](https://github.com/tomsci) | [qrcodegen.c](../../components/modules/qrcodegen.c)|

This module wraps the [QR Code](https://en.wikipedia.org/wiki/QR_code) Generator API written in C by https://github.com/nayuki/QR-Code-generator.git for producing a QR Code.

## qrcodegen.encodeText()
Generates a QR Code from a text string. In the most optimistic case, a QR Code at version 40 with low ECC can hold any UTF-8 string up to 2953 bytes, or any alphanumeric string up to 4296 characters, or any digit string up to 7089 characters.

#### Syntax
`qrcodegen.encodeText(text, [options])`

#### Parameters
- `text` The text or URL to encode. Should be UTF-8 or ASCII.
- `options` An optional table, containing any of:
    - `minver` the minimum version according to the QR Code Model 2 standard. If not specified, defaults to `1`.
    - `maxver` the maximum version according to the QR Code Model 2 standard. If not specified, defaults to `40`. Specifying a lower maximum version reduces the amount of temporary memory the function requires, so it can be worthwhile to specify a smaller value if you know the `text` will fit in a lower-version QR Code.
    - `ecl` the error correction level in a QR Code symbol. Higher error correction produces a larger QR Code. One of:
        - `qrcodegen.LOW` (default if not specified)
        - `qrcodegen.MEDIUM`
        - `qrcodegen.QUARTILE`
        - `qrcodegen.HIGH`
    - `mask` the mask pattern used in a QR Code symbol. An integer 0-7, or `qrcodegen.AUTO` (the default).
    - `boostecl` defaults to false.

#### Returns
The QR Code, encoded as a string. Use `qrcodegen.getSize()` and `qrcodegen.getPixel()` to extract data from the result. If the text cannot be represented within the given version range (for example it is too long) then `nil` is returned.

#### Example
```lua
qrcode = qrcodegen.encodeText("https://nodemcu.readthedocs.io/", {minver=1, maxver=4})
print("Size:", qrcodegen.getSize(qrcode))
```

## qrcodegen.getSize()

#### Syntax
`qrcodegen.getSize(qrcode)`

#### Parameters
- `qrcode` a QR Code string, as returned by `qrcodegen.encodeText()`.

#### Returns
Returns the side length in pixels of the given QR Code. The result is in the range [21, 177].

## qrcodegen.getPixel(qrcode, x, y)
Get the color of the pixel at the given coordinates of the QR Code. `x` and `y` must be between `0` and the value returned by `qrcodegen.getSize()`.

#### Syntax
`qrcodegen.getPixel(qrcode, x, y)`

#### Parameters
- `qrcode` a QR Code string, as returned by `qrcodegen.encodeText()`.
- `x`
- `y`

#### Returns
`true` if the given pixel is black, `false` if it is white.

#### Example
```lua
qrcode = qrcodegen.encodeText("https://nodemcu.readthedocs.io/", {maxver=4})
size = qrcodegen.getSize(qrcode)
for y = 0, size-1 do
    local row = {}
    for x = 0, size-1 do
        row[x + 1] = qrcodegen.getPixel(x, y) and "*" or " "
    end
    print(table.concat(row))
end
```
