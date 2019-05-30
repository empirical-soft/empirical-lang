These tests cover error reporting.

### Preliminary setup

```
>>> let prices = load$("sample_csv/prices.csv")

>>> var my_int = 0

```

### Parse errors

```
>>> 'Hello'
Error: character must have exactly one item

>>> join prices, events on symbol on name
Error: 'on' already listed

```

These messages should be improved.

```
>>> from prices select where symbol == "AAPL", volume > 30000000
Error: unable to parse

>>> 1 +
Error: unable to parse

>>> (
Error: unable to parse

```

### Type errors

```
>>> 1 + 2.
Error: unable to match overloaded function +
  candidate: (Int64, Int64) -> Int64
    argument type at position 1 does not match: Float64 vs Int64
  candidate: (Float64, Float64) -> Float64
    argument type at position 0 does not match: Int64 vs Float64
  candidate: (Int64, [Int64]) -> [Int64]
    argument type at position 1 does not match: Float64 vs [Int64]
  ...
  <45 others>

>>> my_int = 7.
Error: mismatched types in assignment: Int64 vs Float64

```

### Identifier errors

```
>>> my_float = 0.0
Error: symbol my_float was not found

```
