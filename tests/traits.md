These tests cover traits and modes.

### Literal

```
>>> traits_of(5)
pure, transform, linear

>>> traits_of(print(5))
none

>>> traits_of(range)
pure, transform, linear

>>> traits_of(range(5))
pure, transform, linear

>>> mode_of(5)
Comptime

>>> mode_of(print(5))
Normal

>>> mode_of(range(5))
Comptime

```

### Variable

```
>>> let x = 7

>>> var y = 7

>>> traits_of(x + 3)
pure, transform, linear

>>> traits_of(y + 3)
none

>>> mode_of(x + 3)
Comptime

>>> mode_of(y + 3)
Normal

```

### List

```
>>> traits_of([1, 2, 3])
pure, transform, linear

>>> traits_of([1, x, 3])
pure, transform, linear

>>> traits_of([1, y, 3])
none

>>> mode_of([1, 2, 3])
Comptime

>>> mode_of([1, x, 3])
Comptime

>>> mode_of([1, y, 3])
Normal

```

### Subscript

```
>>> let xs = [1, 2, 3]

>>> var ys = [1, 2, 3]

>>> traits_of(xs[0])
pure, transform

>>> traits_of(xs[x])
pure, transform

>>> traits_of(xs[y])
none

>>> traits_of(ys[0])
none

>>> traits_of(ys[x])
none

>>> traits_of(ys[y])
none

>>> mode_of(xs[0])
Comptime

>>> mode_of(xs[x])
Comptime

>>> mode_of(xs[y])
Normal

>>> mode_of(ys[0])
Normal

>>> mode_of(ys[x])
Normal

>>> mode_of(ys[y])
Normal

```

### Type

```
>>> data Person: name: String, age: Int64 end

>>> let p = Person("a", 1)

>>> traits_of(Int64)
pure, transform, linear

>>> traits_of(Person)
pure, transform, linear

>>> traits_of(p)
pure, transform, linear

>>> mode_of(Int64)
Comptime

>>> mode_of(Person)
Comptime

>>> mode_of(p)
Comptime

>>> traits_of(Int64("7"))
pure, transform, linear

>>> mode_of(Int64("7"))
Comptime

```

### Member

```
>>> let py = Person("a", y)

>>> traits_of(p.name)
pure, transform, linear

>>> traits_of(py.name)
none

>>> mode_of(p.name)
Comptime

>>> mode_of(py.name)
Normal

```

### Function

```
>>> func inc(x: Int64): return x + 1 end

>>> traits_of(inc)
pure, transform, linear

>>> mode_of(inc)
Comptime

>>> mode_of(inc(5))
Comptime

>>> mode_of(inc(x))
Comptime

>>> mode_of(inc(y))
Normal

```

### Dataframe

```
>>> let prices = load("sample_csv/prices.csv")

>>> traits_of(prices)
transform, linear

>>> traits_of(prices.volume)
transform, linear

>>> traits_of(prices.volume[0])
transform

>>> traits_of(prices.volume + 1)
transform, linear

>>> traits_of(print(prices.volume + 1))
none

>>> mode_of(prices)
Normal

>>> mode_of(prices.volume)
Normal

>>> mode_of(prices.volume[0])
Normal

>>> mode_of(prices.volume + 1)
Normal

>>> mode_of(print(prices.volume + 1))
Normal

```

### Streaming Dataframe

```
>>> let stream_prices = stream_load("sample_csv/prices.csv")

>>> traits_of(stream_prices)
transform, linear

>>> traits_of(stream_prices.volume)
transform, linear

>>> traits_of(stream_prices.volume[0])
transform

>>> traits_of(stream_prices.volume + 1)
transform, linear

>>> traits_of(print(stream_prices.volume + 1))
none

>>> mode_of(stream_prices)
Stream

>>> mode_of(stream_prices.volume)
Stream

>>> mode_of(stream_prices.volume[0])
Normal

>>> mode_of(stream_prices.volume + 1)
Stream

>>> mode_of(print(stream_prices.volume + 1))
Normal

```

### Table syntax

```
>>> mode_of(from stream_prices select sum(volume) by symbol)
Stream

>>> mode_of(sort stream_prices by volume)
Normal

```

### Miscellaneous

```
>>> mode_of(now())
Normal

```
