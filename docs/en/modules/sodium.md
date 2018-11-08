# Sodium module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2018-10-27 | [Tom Sutcliffe](https://github.com/tomsci) | [Tom Sutcliffe](https://github.com/tomsci) | [sodium.c](../../../components/modules/sodium.c)|

This module wraps the [LibSodium](https://libsodium.org/) C library. LibSodium is a library for performing Elliptic Curve Cryptography.

!!! note

    Almost all functions in this module require a working random number generator. On the ESP32 this means that *WiFi must be started* otherwise ALL OF THE CRYPTOGRAPHY WILL SILENTLY BE COMPROMISED. Make sure to call `wifi.start()` before any of the functions in this module. The only exception is `crypto_box_seal_open()` which does not require a random number source to operate.

# Random number generation
See also [https://download.libsodium.org/doc/generating_random_data](https://download.libsodium.org/doc/generating_random_data)

## sodium.randombytes_random()
Returns a random integer between `0` and `0xFFFFFFFF` inclusive. Note that on a build using `LUA_NUMBER_INTEGRAL`, results may appear negative due to integer overflow. Wifi must be started, by calling `wifi.start()`, before calling this function.

#### Syntax
`sodium.randombytes_random()`

#### Parameters
None

#### Returns
A uniformly-distributed random integer between `0` and `0xFFFFFFFF` inclusive.

## sodium.randombytes_uniform()
Returns a random integer  `0 <= result < upper_bound`. Unlike `randombytes_random() % upper_bound`, it guarantees a uniform distribution of the possible output values even when `upper_bound` is not a power of 2. Note that on a build using `LUA_NUMBER_INTEGRAL`, if `upper_bound >= 0x80000000` the result may appear negative due to integer overflow. Wifi must be started, by calling `wifi.start()`, before calling this function.

#### Syntax
`sodium.randombytes_uniform(upper_bound)`

#### Parameters
- `upper_bound` must be an integer `<= 0xFFFFFFFF`.

#### Returns
An integer `>= 0` and `< upper_bound`

## sodium.randombytes_buf()
Generates `n` bytes of random data. Wifi must be started, by calling `wifi.start()`, before calling this function.

#### Syntax
`sodium.randombytes_buf(n)`

#### Parameters
- `n` number of bytes to return.

#### Returns
A string of `n` random bytes.

# Generating public and secret keys
The keys created by `crypto_box_keypair()` can be used the `crypto_box_seal*()` functions.

## sodium.crypto_box_keypair()
Generates a new keypair. Wifi must be started, by calling `wifi.start()`, before calling this function.

#### Parameters
None

#### Returns
Two values, `public_key, secret_key`. Both are strings (although containing non-printable characters).

#### Example
```lua
public_key, secret_key = sodium.crypto_box_keypair()
```

# Sealed box public key cryptography
See also [https://download.libsodium.org/doc/public-key_cryptography/sealed_boxes](https://download.libsodium.org/doc/public-key_cryptography/sealed_boxes).

## sodium.crypto_box_seal()
Encrypts a message using a public key, such that only someone knowing the corresponding secret key can decrypt it using [`crypto_box_seal_open()`](#sodiumcryptoboxsealopen). This API does not store any information about who encrypted the message, therefore at the point of decryption there is is no proof the message hasn't been tampered with or sent by somone else. Wifi must be started, by calling `wifi.start()`, before calling this function.

#### Syntax
`sodium.crypto_box_seal(message, public_key)`

#### Parameters
- `message` - the string to encrypt.
- `public_key` - the public key to encrypt with.

#### Returns
The encrypted message, as a string. Errors if `public_key` is not a valid public key as returned by `crypto_box_keypair()` or if the message could not be encrypted.

#### Example
```lua
ciphertext = sodium.crypto_box_seal(message, public_key)
```

## sodium.crypto_box_seal_open
Decrypts a message encrypted with [`crypto_box_seal()`](#sodiumcryptoboxseal).

#### Syntax
`sodium.crypto_box_seal_open(ciphertext, public_key, secret_key)`

#### Parameters
- `ciphertext` - the encrypted message.
- `public_key` - the public key the message was encrypted with.
- `secret_key` - the secret key corresponding to the specified public key.

#### Returns
The decrypted plain text of the message. Returns `nil` if the `ciphertext` could not be decrypted.

#### Example
```lua
message = sodium.crypto_box_seal_open(ciphertext, public_key, secret_key)
```
