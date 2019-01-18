# crypto Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-01-13 | [Javier Peletier](https://github.com/jpeletier) | [Javier Peletier](https://github.com/jpeletier) | [crypto.c](../../../components/modules/crypto.c)|

The crypto module provides various functions for working with cryptographic algorithms.

This is work in progress, for now only a number of hashing functions are available:

* SHA1
* SHA256
* SHA224
* SHA512
* SHA384
* MD5

All except MD5 are enabled by default. To disable algorithms you don't need, find the "Crypto module hashing algorithms" under the NodeMCU modules section in menuconfig.

## crypto.new_hash()

Create a digest/hash object that can have any number of strings added to it. Object has `update` and `finalize` functions.

#### Syntax
`hashobj = crypto.new_hash(algo)`

#### Parameters
`algo` the hash algorithm to use, case insensitive string

#### Returns
Hasher object with `update` and `finalize` functions available.

#### Example
```lua
hashobj = crypto.new_hash("SHA1")
hashobj:update("FirstString"))
hashobj:update("SecondString"))
digest = hashobj:finalize()
print(encoder.toHex(digest))
```