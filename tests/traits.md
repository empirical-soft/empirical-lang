These tests cover traits and modes.

### Literal

```
>>> traits_of(5)
pure, transform, linear

>>> traits_of(print)
none

>>> traits_of(print(5))
none

>>> traits_of(range)
pure, transform, linear, autostream

>>> traits_of(range(5))
pure, transform, linear

>>> mode_of(5)
Comptime

>>> mode_of(print(5))
Normal

>>> mode_of(range(5))
Stream

```

### List

```
>>> traits_of([1, 2, 3])
pure, transform, linear

>>> mode_of([1, 2, 3])
Comptime

```

#### Variable

```
>>> let x = 7

>>> traits_of(x + 3)
pure, transform, linear

>>> mode_of(x + 3)
Comptime

```

### Type

```
>>> traits_of(Int64)
none

>>> mode_of(Int64)
Normal

```

### Function

```
>>> func inc(x: Int64): return x + 1 end

>>> traits_of(inc)
pure, transform, linear

>>> mode_of(inc(5))
Comptime

```

### Dataframe

```
>>> let prices = load$("sample_csv/prices.csv")

>>> traits_of(prices)
transform, linear

>>> traits_of(prices.volume)
transform, linear

>>> traits_of(prices.volume + 1)
transform, linear

>>> traits_of(print(prices.volume + 1))
none

>>> mode_of(prices)
Stream

>>> mode_of(prices.volume)
Stream

>>> mode_of(prices.volume + 1)
Stream

>>> mode_of(print(prices.volume + 1))
Normal

```

### Table syntax

```
>>> mode_of(from prices select sum(volume) by symbol)
Stream

>>> mode_of(sort prices by volume)
Normal

```
