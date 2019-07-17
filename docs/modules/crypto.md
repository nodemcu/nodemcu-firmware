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

## crypto.new_hash()

Create a digest/hash object that can have any number of strings added to it.

The returned object has `update(str)` and `finalize()` functions, for
streaming data through the hash function, and for finalizing and returning
the resulting digest.

#### Syntax
`hashobj = crypto.new_hash(algo)`

#### Parameters
- `algo` the hash algorithm to use, case insensitive string

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
