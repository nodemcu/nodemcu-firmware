# crypto Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-01-13 | [Javier Peletier](https://github.com/jpeletier), [Johny Mattsson](https://github.com/jmattsson) | [Javier Peletier](https://github.com/jpeletier) | [crypto.c](../../../components/modules/crypto.c)|

The crypto module provides various functions for working with cryptographic algorithms.

The following algorithms are supported, both in digest mode and in HMAC mode:

* MD5
* SHA1
* RIPEMD160
* SHA224
* SHA256
* SHA384
* SHA512

The following algorithms are supported only in digest (hash) mode:

* BLAKE2b

The BLAKE2b hash algorithm is only available if the ROM has been built with the Sodium module enabled.

## crypto.hash()

Compute a cryptographic hash of a Lua string.

#### Syntax
`hash = crypto.hash(algo, str)`

or if `algo` is `"BLAKE2b"`:

`hash = crypto.hash(algo, str, [key[, out_len])`

#### Parameters
- `algo` the hash algorithm to use, case insensitive string
- `str` string to hash contents of
- `key` only applicable if `algo` is `"BLAKE2b"`, optional. See [`sodium.generichash()`](sodium.md#sodiumgenerichash) for more info.
- `out_len` only applicable if `algo` is `"BLAKE2b"`, optional. See [`sodium.generichash()`](sodium.md#sodiumgenerichash) for more info.

#### Returns
A binary string containing the message digest. To obtain the textual version (ASCII hex characters), use [`encoder.toHex()`](encoder.md#encodertohex).

#### Example
```lua
print(encoder.toHex(crypto.hash("SHA1", "abc")))
```

#### See also
[`crypto.new_hash()`](#cryptonew_hash) if the entire data to be hashed is not available in a single string.


## crypto.new_hash()

Create a digest/hash object that can have any number of strings added to it.

The returned object has `update(str)` and `finalize()` functions, for
streaming data through the hash function, and for finalizing and returning
the resulting digest.

#### Syntax
`hashobj = crypto.new_hash(algo)`

or if `algo` is `"BLAKE2b"`:

`hashobj = crypto.new_hash(algo, [key[, out_len])`

#### Parameters
- `algo` the hash algorithm to use, case insensitive string
- `key` only applicable if `algo` is `"BLAKE2b"`, optional. See [`sodium.generichash()`](sodium.md#sodiumgenerichash) for more info.
- `out_len` only applicable if `algo` is `"BLAKE2b"`, optional. See [`sodium.generichash()`](sodium.md#sodiumgenerichash) for more info.

#### Returns
Hasher object with `update` and `finalize` functions available.

#### Example
```lua
hashobj = crypto.new_hash("SHA1")
hashobj:update("FirstString")
hashobj:update("SecondString")
digest = hashobj:finalize()
print(encoder.toHex(digest))
```


## crypto.hmac()

Calculate a HMAC (Hashed Message Authentication Code, aka "signature"). A HMAC
can be used to simultaneously verify both integrity and authenticity of data.

#### Syntax
`hmac = crypto.hmac(algo, key, data)`

#### Parameters
- `algo` the hash algorithm to use, case insensitive string
- `key` the signing key (may be a binary string)
- `data` the data to calculate the HMAC for

#### Returns
A binary string containing the signature. To obtain the textual version (ASCII hex characters), use [`encoder.toHex()`](encoder.md#encodertohex).

#### Example
```lua
signature = crypto.hmac("SHA1", "top s3cr3t key", "Some data")
print(encoder.toHex(signature))
```

#### See also
[`crypto.new_hmac()`](#cryptonew_hmac) if the entire data to be hashed is not available in a single string.


## crypto.new_hmac()

Create an object for calculating a HMAC (Hashed Message Authentication Code,
aka "signature"). A HMAC can be used to simultaneously verify both integrity
and authenticity of data.

The returned object has `update(str)` and `finalize()` functions, for
streaming data through the hash function, and for finalizing and returning
the resulting signature.


#### Syntax
`hashobj = crypto.new_hmac(algo, key)`

#### Parameters
- `algo` the hash algorithm to use, case insensitive string
- `key` the signing key (may be a binary string)

#### Returns
Hasher object with `update` and `finalize` functions available.

#### Example
```lua
hmac = crypto.new_hmac("SHA1", "top s3cr3t key")
hmac:update("Some data")
hmac:update("... more data")
signature = hmac:finalize()
print(encoder.toHex(signature))
```
