# crypto Module

The crypto modules provides various functions for working with cryptographic algorithms.

## crypto.hash()

Compute a cryptographic hash of a Lua string.

#### Syntax
`hash = crypto.hash(algo, str)`

#### Parameters
`algo` the hash algorithm to use, case insensitive string

Supported hash algorithms are:

- MD2 (not available by default, has to be explicitly enabled in `app/include/user_config.h`)
- MD5
- SHA1
- SHA256, SHA384, SHA512 (unless disabled in `app/include/user_config.h`)

#### Returns
A binary string containing the message digest. To obtain the textual version (ASCII hex characters), please use [`crypto.toHex()`](#cryptotohex	).

#### Example
```lua
print(crypto.toHex(crypto.hash("sha1","abc")))
```

## crypto.hmac()

Compute a [HMAC](https://en.wikipedia.org/wiki/Hash-based_message_authentication_code) (Hashed Message Authentication Code) signature for a Lua string.

#### Syntax
`signature = crypto.hmac(algo, str, key)`

#### Parameters
- `algo` hash algorithm to use, case insensitive string
- `str` data to calculate the hash for
- `key` key to use for signing, may be a binary string

Supported hash algorithms are:

- MD2 (not available by default, has to be explicitly enabled in `app/include/user_config.h`)
- MD5
- SHA1
- SHA256, SHA384, SHA512 (unless disabled in `app/include/user_config.h`)

#### Returns
A binary string containing the HMAC signature. Use [`crypto.toHex()`](#cryptotohex	) to obtain the textual version.

#### Example
```lua
print(crypto.toHex(crypto.hmac("sha1","abc","mysecret")))
```

## crypto.mask()

Applies an XOR mask to a Lua string. Note that this is not a proper cryptographic mechanism, but some protocols may use it nevertheless.

#### Syntax
`crypto.mask(message, mask)`

#### Parameters
- `message` message to mask
- `mask` the mask to apply, repeated if shorter than the message

#### Returns
The masked message, as a binary string. Use [`crypto.toHex()`](#cryptotohex) to get a textual representation of it.

#### Example
```lua
print(crypto.toHex(crypto.mask("some message to obscure","X0Y7")))
```

## crypto.toBase64()

Provides a Base64 representation of a (binary) Lua string.

#### Syntax
`b64 = crypto.toBase64(binary)`

#### Parameters
`binary` input string to Base64 encode

#### Return
A Base64 encoded string.

#### Example
```lua
print(crypto.toBase64(crypto.hash("sha1","abc")))
```

## crypto.toHex()

Provides an ASCII hex representation of a (binary) Lua string. Each byte in the input string is represented as two hex characters in the output.

#### Syntax
`hexstr = crypto.toHex(binary)`

#### Parameters
`binary` input string to get hex representation for

#### Returns
An ASCII hex string.

#### Example
```lua
print(crypto.toHex(crypto.hash("sha1","abc")))
```
