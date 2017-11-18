# Bloom Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-11-13 | [Philip Gladstone](https://github.com/pjsg) | [Philip Gladstone](https://github.com/pjsg) | [bloom.c](../../../app/modules/bloom.c)|


This module implements a [Bloom filter](https://en.wikipedia.org/wiki/Bloom_filter). This is a probabilistic data structure that is used to test for set membership. There are two operations -- `add` and `check` that allow 
arbitrary strings to be added to the set or tested for set membership. Since this is a probabilistic data structure, the answer returned can be incorrect. However,
if the string *is* a member of the set, then the `check` operation will always return `true`. 

## bloom.create()
Create a filter object.

#### Syntax
`bloom.create(elements, errorrate)`

#### Parameters
- `elements` The largest number of elements to be added to the filter.
- `errorrate` The error rate (the false positive rate). This is represented as `n` where the false positive rate is `1 / n`. This is the maximum rate of `check` returning true when the string is *not* in the set.

#### Returns
A `filter` object.

#### Example

```
    filter = bloom.create(10000, 100)    -- this will use around 11kB of memory
```

## filter:add()
Adds a string to the set and returns an indication of whether the string was already present.

#### Syntax
`filter:add(string)`

#### Parameters
- `string` The string to be added to the filter set.

#### Returns
`true` if the string was already present in the filter. `false` otherwise.

#### Example

```
    if filter:add("apple") then
      print ("Seen an apple before!")
    else
      print ("Noted that the first apple has been seen")
    end
```

## filter:check()
Checks to see if a string is present in the filter set.

#### Syntax
`present = filter:check(string)`

#### Parameters
- `string` The string to be checked for membership in the set.

#### Returns
`true` if the string was already present in the filter. `false` otherwise.

#### Example

```
    if filter:check("apple") then
      print ("Seen an apple before!")
    end
```


## filter:reset()
Empties the filter.

#### Syntax
`filter:reset()`

#### Returns
Nothing

#### Example
```
filter:reset()
```

## filter:info()
Get some status information on the filter.

#### Syntax
`bits, fns, occupancy, fprate = filter:info()`

#### Returns
- `bits` The number of bits in the filter.
- `fns` The number of hash functions in use.
- `occupancy` The number of bits set in the filter. 
- `fprate` The approximate chance that the next `check` will return `true` when it should return `false`. This is represented as the inverse of the probability -- i.e. as the n in 1-in-n chance. This value is limited to 1,000,000.

#### Example
```
bits, fns, occupancy, fprate = filter:info()
```

