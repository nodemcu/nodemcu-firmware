# Sodium module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2018-10-27 | [Tom Sutcliffe](https://github.com/tomsci) | [Tom Sutcliffe](https://github.com/tomsci) | [sodium.c](../../components/modules/sodium.c)|

This module wraps the [LibSodium](https://libsodium.org/) C library. LibSodium is a library for performing Elliptic Curve Cryptography.

In addition to the flag for enabling this module during ROM build, `Component config -> NodeMCU Modules -> Sodium module`, there are additional settings for libsodium under `Component config -> libsodium`.

!!! note

    Almost all functions in this module require a working random number generator. On the ESP32 this means that *WiFi must be started* otherwise ALL OF THE CRYPTOGRAPHY WILL SILENTLY BE COMPROMISED. Make sure to call `wifi.start()` before any of the functions in this module. See the [Espressif documentation](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/system/system.html#random-number-generation) for more information. The only exceptions are `sodium.crypto_box.seal_open()` and the `generichash` APIs which do not require a random number source to operate.

There are several parts to this module:

- Random number generation, using the `sodium.random...` APIs
    - See also [https://download.libsodium.org/doc/generating_random_data](https://download.libsodium.org/doc/generating_random_data)
- Hashing, using the `sodium.generichash...` APIs.
    - The hash functions are implemented using BLAKE2b, a simple, standardized ([RFC 7693](https://www.rfc-editor.org/rfc/rfc7693.txt)) secure hash function that is as strong as SHA-3 but faster than SHA-1 and MD5.
    - See also [https://download.libsodium.org/doc/hashing/generic_hashing](https://download.libsodium.org/doc/hashing/generic_hashing).
- Sealed box public key cryptography, using the `sodium.crypto_box...` APIs
    - See also [https://download.libsodium.org/doc/public-key_cryptography/sealed_boxes](https://download.libsodium.org/doc/public-key_cryptography/sealed_boxes).

## sodium.random.random()
Returns a random integer between `0` and `0xFFFFFFFF` inclusive. Note that on a build using `LUA_NUMBER_INTEGRAL`, results may appear negative due to integer overflow. Wifi must be started, by calling `wifi.start()`, before calling this function.

#### Syntax
`sodium.random.random()`

#### Parameters
None

#### Returns
A uniformly-distributed random integer between `0` and `0xFFFFFFFF` inclusive.

## sodium.random.uniform()
Returns a random integer  `0 <= result < upper_bound`. Unlike `sodium.random.random() % upper_bound`, it guarantees a uniform distribution of the possible output values even when `upper_bound` is not a power of 2. Note that on a build using `LUA_NUMBER_INTEGRAL`, if `upper_bound >= 0x80000000` the result may appear negative due to integer overflow. Wifi must be started, by calling `wifi.start()`, before calling this function.

#### Syntax
`sodium.random.uniform(upper_bound)`

#### Parameters
- `upper_bound` must be an integer `<= 0xFFFFFFFF`.

#### Returns
An integer `>= 0` and `< upper_bound`

## sodium.random.buf()
Generates `n` bytes of random data. Wifi must be started, by calling `wifi.start()`, before calling this function.

#### Syntax
`sodium.random.buf(n)`

#### Parameters
- `n` number of bytes to return.

#### Returns
A string of `n` random bytes.

## sodium.crypto_box.keypair()
Generates a new keypair. Wifi must be started, by calling `wifi.start()`, before calling this function. The keys created by this function can be used by the `crypto_box.seal*()` functions.

#### Parameters
None

#### Returns
Two values, `public_key, secret_key`. Both are strings (although containing non-printable characters).

#### Example
```lua
public_key, secret_key = sodium.crypto_box.keypair()
```

## sodium.crypto_box.seal()
Encrypts a message using a public key, such that only someone knowing the corresponding secret key can decrypt it using [`sodium.crypto_box.seal_open()`](#sodiumcrypto_boxseal_open). This API does not store any information about who encrypted the message, therefore at the point of decryption there is is no proof the message hasn't been tampered with or sent by somone else. Wifi must be started, by calling `wifi.start()`, before calling this function.

#### Syntax
`sodium.crypto_box.seal(message, public_key)`

#### Parameters
- `message` - the string to encrypt.
- `public_key` - the public key to encrypt with.

#### Returns
The encrypted message, as a string. Errors if `public_key` is not a valid public key as returned by `sodium.crypto_box.keypair()` or if the message could not be encrypted.

#### Example
```lua
ciphertext = sodium.crypto_box.seal(message, public_key)
```

## sodium.crypto_box.seal_open()
Decrypts a message encrypted with [`crypto_box.seal()`](#sodiumcrypto_boxseal).

#### Syntax
`sodium.crypto_box.seal_open(ciphertext, public_key, secret_key)`

#### Parameters
- `ciphertext` - the encrypted message.
- `public_key` - the public key the message was encrypted with.
- `secret_key` - the secret key corresponding to the specified public key.

#### Returns
The decrypted plain text of the message. Returns `nil` if the `ciphertext` could not be decrypted.

#### Example
```lua
message = sodium.crypto_box.seal_open(ciphertext, public_key, secret_key)
```

## sodium.generichash()
Computes a hash of the given data. The same data will always produce the same hash (providing `key` and `out_len` parameters are the same).

#### Syntax
`sodium.generichash(data[, key[, out_len]])`

#### Parameters
- `data` - the data to hash
- `key` - Optional. A string to use as the key. A key parameter can be used for example to make sure that different applications generate different hashes even if they process the same data. If specified, must be between 16 and 64 bytes.
- `out_len` - Optional. The length of hash to output, which must be between 16 and 64. If not specified, defaults to 32 bytes (`crypto_generichash_BYTES`).

#### Returns
The hash of the data, as an unencoded string of `out_len` bytes. Use something like `encoder.toHex(result)` if you need it as ASCII text.

#### Example
```lua
hash = sodium.generichash("Some data")
print("Result:", encoder.toHex(hash))
```

## sodium.generichash_init()
Provides a way to calculate a hash in chunks, so that not all the data needs to be in memory at once.

#### Syntax
`sodium.generichash_init([key[, out_len]])`

#### Parameters
- `key` - Optional. As per [`sodium.generichash()`](#sodiumgenerichash).
- `out_len` - Optional. As per [`sodium.generichash()`](#sodiumgenerichash).

#### Returns
An object with 2 methods, `update(data)` and `final()`. Call `update()` with each data chunk in turn, then call `final()` to fetch the resulting hash. Do not call `update()` again after calling `final()`: to calculate another hash, call `generichash_init()` again to get a new object.

#### Example
```lua
hasher = sodium.generichash_init()
hasher:update(data_chunk_1)
hasher:update(data_chunk_2)
hash = hasher:final()
print("Result:", encoder.toHex(hash))
```
