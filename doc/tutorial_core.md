## Expressions and Variables

Expressions are evaluated and the results are returned to the user.

```
>>> 7 + 31
38

>>> 2 * 3 + 10
16

>>> 0xFF
255

>>> 3.14
3.14

```

Variables are indicated with a `let` (immutable) or `var` (mutable).

```
>>> let x = 1

>>> x + 99
100

>>> var y = x + 99

>>> y
100

>>> y = 3

>>> y
3

```

Types are inferred automatically, but users can denote types explicitly.

```
>>> let pi: Float64 = 3.1415

```

Explicit types are required if no initial value is provided.

```
>>> var user_name: String

>>> user_name = "Charles Babbage"

>>> user_name
"Charles Babbage"

```

If no initial value and no type are provided, then we have an error.

```
>>> var user_age
Error: unable to determine type

```

Similarly, types must match if both an initial value and an explicit type are provided.

```
>>> let e: Int64 = 2.71
Error: type of declaration does not match: Int64 vs Float64

```

## Types

All values have a type, resolved at compile time.

```
>>> let x: Int64 = 37

```

The static typing is strict and prevents common errors.

```
>>> x + 1.0
Error: unable to match overloaded function +
  candidate: (Int64, Int64) -> Int64
    argument type at position 1 does not match: Float64 vs Int64
  candidate: (Float64, Float64) -> Float64
    argument type at position 0 does not match: Int64 vs Float64
  candidate: (Int64, [Int64]) -> [Int64]
    argument type at position 1 does not match: Float64 vs [Int64]
  ...
  <45 others>

```

A value can be cast to a desired type.

```
>>> Float64(x) + 1.0
38.0

```

This kind of explicit typing makes code less error prone.

```
>>> "12" / 2
Error: unable to match overloaded function /
  candidate: (Int64, Int64) -> Int64
    argument type at position 0 does not match: String vs Int64
  candidate: (Float64, Float64) -> Float64
    argument type at position 0 does not match: String vs Float64
  candidate: (Int64, [Int64]) -> [Int64]
    argument type at position 0 does not match: String vs Int64
  ...
  <21 others>

>>> Int64("12") / 2
6

```

If a cast is invalid, then we will have a `nil` (integers) or `nan` (floating point). The "missing data" value is propagated by operators.

```
>>> Int64("12b") / 2
nil

```

Users can define their own values.

```
>>> data Person: name: String, age: Int64 end

>>> let p = Person("Alice", 37)

```

These are displayed as a table by default.

```
>>> p
  name age
 Alice  37

>>> p.name = "Bob"

>>> p
 name age
  Bob  37

```

We can define a type cast if desired.

```
>>> func String(p: Person): return p.name + " is " + String(p.age) + " years old" end

>>> String(p)
"Bob is 37 years old"

```

Prepending a user-defined type with a bang (`!`) changes the type to a Dataframe. All entries will be vectorized.

```
>>> !Person(["Alice", "Bob"], [37, 39])
  name age
 Alice  37
   Bob  39

```

## Arrays

Arrays can be made from any builtin type.

```
>>> [1, 2, 3]
[1, 2, 3]

>>> let xs: [Float64] = [1., 2., 3.]

>>> xs
[1.0, 2.0, 3.0]

```

Applying an operator on a vector with a scalar causes the scalar operator to apply to each vector element.

```
>>> xs * 3.0
[3.0, 6.0, 9.0]

```

Applying an operator between two vectors is an element-wise application.

```
>>> xs * [2., 4., 6.]
[2.0, 8.0, 18.0]

```

Element-wise operations require that the vectors be the same length.

```
>>> xs * [2., 4.]
Error: Mismatch array lengths

```

Arrays of consecutive integers can be created from `range()`.

```
>>> range(100)
[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, ...]

```

## Functions

Functions apply to a list of parameters.

```
>>> func add(x: Int64, y: Int64): return x + y end

>>> add(3, 7)
10

```

The return type is inferred, but may be explicitly listed.

```
>>> func add3(x: Int64, y: Int64, z: Int64) -> Int64: return add(add(x, y), z) end

>>> add3(4, 5, 6)
15

```

Functions can, of course, be recursive.

```
>>> func fac(x: Int64): if x == 0: return 1 else: return x * fac(x - 1) end end

>>> fac(5)
120

```

Functions can be overloaded by parameter type.

```
>>> func add(x: Bool, y: Bool): return x or y end

>>> add(true, false)
true

>>> add(1, 0)
1

```

An error occurs if types don't match during a function call.

```
>>> add(3.4, 5.6)
Error: unable to match overloaded function add
  candidate: (Int64, Int64) -> Int64
    argument type at position 0 does not match: Float64 vs Int64
  candidate: (Bool, Bool) -> Bool
    argument type at position 0 does not match: Float64 vs Bool

```

Operators are just syntactic sugar for a function call.

```
>>> (+)(3, 5)
8

```

Operators can be overloaded.

```
>>> data Point: x: Int64, y: Int64 end

>>> func (+)(p: Point, n: Int64): return Point(p.x + n, p.y + n) end

>>> Point(5, 7) + 12
  x  y
 17 19

```

User-defined literals can be defined by prepending `suffix` to any function name.

```
>>> func suffix_w(x: Int64): return x * 3 end

>>> 7_w
21

>>> 0xFF_w
765

>>> func suffix_z(x: Float64): return 3.0 * x end

>>> 1.2e4_z
36000.0

```

## Control Flow

Loops and conditionals require a boolean expression.

```
>>> let x = 7

>>> x < 1
false

```

Conditional expressions can nest via `elif`.

```
>>> func code(c: String): if c == "red": return 'R' elif c == "blue": return 'B' else: return '?' end end

>>> code("green")
'?'

>>> code("blue")
'B'

>>> code("red")
'R'

```

Loops repeatedly execute until a condition is false.

```
>>> var y = 0

>>> while y < 10: y = y + 1 end

>>> y
10

```

## Timestamps

Timestamps are stored as nanoseconds-since-epoch and are displayed in human-readable form on the console. (We can get the current timestamp via `now()`.)

```
>>> let t1 = Timestamp("2019-03-24 05:58:55.663131")

>>> let t2 = Timestamp("2019-03-24 05:59:18.980145")

```

The difference between two timestamps is a `Timedelta`.

```
>>> t2 - t1
Timedelta("00:00:23.317014")

```

This difference can be added back to any timestamp.

```
>>> let d1 = t2 - t1

>>> t2 + d1
Timestamp("2019-03-24 05:59:42.297159")

```

A `Timedelta` can be represented via a suffix; available units are `ns`, `us`, `ms`, `s`, `m`, `h`, `d`.

```
>>> 5ms
Timedelta("00:00:00.005")

>>> 4h
Timedelta("04:00:00")

```

We can use arithmetic with `Timedelta` to manipulate a timestamp for aggregation.

```
>>> bar(t2, 5m)
Timestamp("2019-03-24 05:55:00")

>>> bar(t2, 1d)
Timestamp("2019-03-24 00:00:00")

```

`Date` and `Time` can isolate specific parts of a timestamp.

```
>>> let date = Date(t2)

>>> let time = Time(t2)

>>> date
Date("2019-03-24")

>>> time
Time("05:59:18.980145")

>>> date + time
Timestamp("2019-03-24 05:59:18.980145")

```

As with any other operator in Empirical, array actions are native.

```
>>> date + [1d, 10d]
[Date("2019-03-25"), Date("2019-04-03")]

>>> 1d * Timedelta([1, 2, 3])
[Timedelta("1 days"), Timedelta("2 days"), Timedelta("3 days")]

>>> bar([t1, t2], 1s)
[Timestamp("2019-03-24 05:58:55"), Timestamp("2019-03-24 05:59:18")]

```

Invalid timestamps are `nil`.

```
>>> Timestamp("err")
Timestamp(nil)

```

## Dataframes

Statically-typed Dataframes (tables) are the primary unique feature of Empirical. If the input source is available at compile time, then the types can be inferred automatically.

```
>>> load$("prices.csv")
 symbol       date   open   high    low  close   volume
   AAPL 2017-01-03 115.80 116.33 114.76 116.15 28781865
   AAPL 2017-01-04 115.85 116.51 115.75 116.02 21118116
   AAPL 2017-01-05 115.92 116.86 115.81 116.61 22193587
   AAPL 2017-01-06 116.78 118.16 116.47 117.91 31751900
   AAPL 2017-01-09 117.95 119.43 117.94 118.99 33561948
   AAPL 2017-01-10 118.77 119.38 118.30 119.11 24462051
   AAPL 2017-01-11 118.74 119.93 118.60 119.75 27588593
  BRK.B 2017-01-03 164.34 164.71 162.44 163.83  4090967
  BRK.B 2017-01-04 164.45 164.57 163.00 164.08  3568919
  BRK.B 2017-01-05 164.06 164.14 162.18 163.30  2982464
  BRK.B 2017-01-06 163.44 163.80 162.64 163.41  2697027
  BRK.B 2017-01-09 163.04 163.25 162.02 162.02  3564674
  BRK.B 2017-01-10 162.00 162.74 161.41 161.47  2671259
  BRK.B 2017-01-11 161.54 162.45 161.03 162.23  3305859
   EBAY 2017-01-03  29.83  30.19  29.64  29.84  7665031
   EBAY 2017-01-04  29.91  30.01  29.51  29.76  9538779
   EBAY 2017-01-05  29.73  30.08  29.61  30.01  9062195
   EBAY 2017-01-06  29.97  31.16  29.78  31.05 13351423
   EBAY 2017-01-09  31.00  31.03  30.60  30.75 10532655
   EBAY 2017-01-10  30.67  30.72  29.84  30.25 13833143
   EBAY 2017-01-11  30.30  30.42  30.01  30.41  8168999

```

Dataframes are just values. They can be assigned to a variable, for example.

```
>>> let prices = load$("prices.csv")

```

Dataframes allow for convenient queries and aggregations.

```
>>> from prices select where date <= Date("2017-01-05")
 symbol       date   open   high    low  close   volume
   AAPL 2017-01-03 115.80 116.33 114.76 116.15 28781865
   AAPL 2017-01-04 115.85 116.51 115.75 116.02 21118116
   AAPL 2017-01-05 115.92 116.86 115.81 116.61 22193587
  BRK.B 2017-01-03 164.34 164.71 162.44 163.83  4090967
  BRK.B 2017-01-04 164.45 164.57 163.00 164.08  3568919
  BRK.B 2017-01-05 164.06 164.14 162.18 163.30  2982464
   EBAY 2017-01-03  29.83  30.19  29.64  29.84  7665031
   EBAY 2017-01-04  29.91  30.01  29.51  29.76  9538779
   EBAY 2017-01-05  29.73  30.08  29.61  30.01  9062195

>>> from prices select sum(volume) by symbol
 symbol    volume
   AAPL 189458060
  BRK.B  22881169
   EBAY  72152225

```

We can combine queries and aggregations.

```
>>> from prices select sum(volume) by symbol where date <= Date("2017-01-05")
 symbol   volume
   AAPL 72093568
  BRK.B 10642350
   EBAY 26266005

```

We can use any expression we want during a query or aggregation.

```
>>> from prices select sum(volume) by symbol, up = open < close
 symbol    up    volume
   AAPL  true 189458060
  BRK.B false  19575310
  BRK.B  true   3305859
   EBAY  true  38247648
   EBAY false  33904577

```

Dataframe columns are simply arrays.

```
>>> prices.open
[115.8, 115.85, 115.92, 116.78, 117.95, 118.77, 118.74, 164.34, 164.45, 164.06, 163.44, 163.04, 162.0, 161.54, 29.83, 29.91, 29.73, 29.97, 31.0, 30.67, 30.3]

```

We can call functions on columns.

```
>>> func mid(xs: [Float64], ys: [Float64]): return (xs + ys) / 2.0 end

>>> from prices select symbol, date, midpoint = mid(low, high)
 symbol       date midpoint
   AAPL 2017-01-03  115.545
   AAPL 2017-01-04  116.130
   AAPL 2017-01-05  116.335
   AAPL 2017-01-06  117.315
   AAPL 2017-01-09  118.685
   AAPL 2017-01-10  118.840
   AAPL 2017-01-11  119.265
  BRK.B 2017-01-03  163.575
  BRK.B 2017-01-04  163.785
  BRK.B 2017-01-05  163.160
  BRK.B 2017-01-06  163.220
  BRK.B 2017-01-09  162.635
  BRK.B 2017-01-10  162.075
  BRK.B 2017-01-11  161.740
   EBAY 2017-01-03   29.915
   EBAY 2017-01-04   29.760
   EBAY 2017-01-05   29.845
   EBAY 2017-01-06   30.470
   EBAY 2017-01-09   30.815
   EBAY 2017-01-10   30.280
   EBAY 2017-01-11   30.215

```

### Sorting

We can sort the Dataframe.

```
>>> sort prices by date
 symbol       date   open   high    low  close   volume
   AAPL 2017-01-03 115.80 116.33 114.76 116.15 28781865
  BRK.B 2017-01-03 164.34 164.71 162.44 163.83  4090967
   EBAY 2017-01-03  29.83  30.19  29.64  29.84  7665031
   AAPL 2017-01-04 115.85 116.51 115.75 116.02 21118116
  BRK.B 2017-01-04 164.45 164.57 163.00 164.08  3568919
   EBAY 2017-01-04  29.91  30.01  29.51  29.76  9538779
   AAPL 2017-01-05 115.92 116.86 115.81 116.61 22193587
  BRK.B 2017-01-05 164.06 164.14 162.18 163.30  2982464
   EBAY 2017-01-05  29.73  30.08  29.61  30.01  9062195
   AAPL 2017-01-06 116.78 118.16 116.47 117.91 31751900
  BRK.B 2017-01-06 163.44 163.80 162.64 163.41  2697027
   EBAY 2017-01-06  29.97  31.16  29.78  31.05 13351423
   AAPL 2017-01-09 117.95 119.43 117.94 118.99 33561948
  BRK.B 2017-01-09 163.04 163.25 162.02 162.02  3564674
   EBAY 2017-01-09  31.00  31.03  30.60  30.75 10532655
   AAPL 2017-01-10 118.77 119.38 118.30 119.11 24462051
  BRK.B 2017-01-10 162.00 162.74 161.41 161.47  2671259
   EBAY 2017-01-10  30.67  30.72  29.84  30.25 13833143
   AAPL 2017-01-11 118.74 119.93 118.60 119.75 27588593
  BRK.B 2017-01-11 161.54 162.45 161.03 162.23  3305859
   EBAY 2017-01-11  30.30  30.42  30.01  30.41  8168999

```

We can sort on multiple columns.

```
>>> sort prices by date, volume
 symbol       date   open   high    low  close   volume
  BRK.B 2017-01-03 164.34 164.71 162.44 163.83  4090967
   EBAY 2017-01-03  29.83  30.19  29.64  29.84  7665031
   AAPL 2017-01-03 115.80 116.33 114.76 116.15 28781865
  BRK.B 2017-01-04 164.45 164.57 163.00 164.08  3568919
   EBAY 2017-01-04  29.91  30.01  29.51  29.76  9538779
   AAPL 2017-01-04 115.85 116.51 115.75 116.02 21118116
  BRK.B 2017-01-05 164.06 164.14 162.18 163.30  2982464
   EBAY 2017-01-05  29.73  30.08  29.61  30.01  9062195
   AAPL 2017-01-05 115.92 116.86 115.81 116.61 22193587
  BRK.B 2017-01-06 163.44 163.80 162.64 163.41  2697027
   EBAY 2017-01-06  29.97  31.16  29.78  31.05 13351423
   AAPL 2017-01-06 116.78 118.16 116.47 117.91 31751900
  BRK.B 2017-01-09 163.04 163.25 162.02 162.02  3564674
   EBAY 2017-01-09  31.00  31.03  30.60  30.75 10532655
   AAPL 2017-01-09 117.95 119.43 117.94 118.99 33561948
  BRK.B 2017-01-10 162.00 162.74 161.41 161.47  2671259
   EBAY 2017-01-10  30.67  30.72  29.84  30.25 13833143
   AAPL 2017-01-10 118.77 119.38 118.30 119.11 24462051
  BRK.B 2017-01-11 161.54 162.45 161.03 162.23  3305859
   EBAY 2017-01-11  30.30  30.42  30.01  30.41  8168999
   AAPL 2017-01-11 118.74 119.93 118.60 119.75 27588593

```

We can sort by an expression.

```
>>> sort prices by symbol, close - open
 symbol       date   open   high    low  close   volume
   AAPL 2017-01-04 115.85 116.51 115.75 116.02 21118116
   AAPL 2017-01-10 118.77 119.38 118.30 119.11 24462051
   AAPL 2017-01-03 115.80 116.33 114.76 116.15 28781865
   AAPL 2017-01-05 115.92 116.86 115.81 116.61 22193587
   AAPL 2017-01-11 118.74 119.93 118.60 119.75 27588593
   AAPL 2017-01-09 117.95 119.43 117.94 118.99 33561948
   AAPL 2017-01-06 116.78 118.16 116.47 117.91 31751900
  BRK.B 2017-01-09 163.04 163.25 162.02 162.02  3564674
  BRK.B 2017-01-05 164.06 164.14 162.18 163.30  2982464
  BRK.B 2017-01-10 162.00 162.74 161.41 161.47  2671259
  BRK.B 2017-01-03 164.34 164.71 162.44 163.83  4090967
  BRK.B 2017-01-04 164.45 164.57 163.00 164.08  3568919
  BRK.B 2017-01-06 163.44 163.80 162.64 163.41  2697027
  BRK.B 2017-01-11 161.54 162.45 161.03 162.23  3305859
   EBAY 2017-01-10  30.67  30.72  29.84  30.25 13833143
   EBAY 2017-01-09  31.00  31.03  30.60  30.75 10532655
   EBAY 2017-01-04  29.91  30.01  29.51  29.76  9538779
   EBAY 2017-01-03  29.83  30.19  29.64  29.84  7665031
   EBAY 2017-01-11  30.30  30.42  30.01  30.41  8168999
   EBAY 2017-01-05  29.73  30.08  29.61  30.01  9062195
   EBAY 2017-01-06  29.97  31.16  29.78  31.05 13351423

```

### Joins

We can join two Dataframes.

```
>>> data Listing: symbol: String, exch: Char end

>>> let listings = !Listing(["AAPL", "BRK.B", "EBAY"], ['Q', 'N', 'Q'])

>>> join prices, listings on symbol
 symbol       date   open   high    low  close   volume exch
   AAPL 2017-01-03 115.80 116.33 114.76 116.15 28781865    Q
   AAPL 2017-01-04 115.85 116.51 115.75 116.02 21118116    Q
   AAPL 2017-01-05 115.92 116.86 115.81 116.61 22193587    Q
   AAPL 2017-01-06 116.78 118.16 116.47 117.91 31751900    Q
   AAPL 2017-01-09 117.95 119.43 117.94 118.99 33561948    Q
   AAPL 2017-01-10 118.77 119.38 118.30 119.11 24462051    Q
   AAPL 2017-01-11 118.74 119.93 118.60 119.75 27588593    Q
  BRK.B 2017-01-03 164.34 164.71 162.44 163.83  4090967    N
  BRK.B 2017-01-04 164.45 164.57 163.00 164.08  3568919    N
  BRK.B 2017-01-05 164.06 164.14 162.18 163.30  2982464    N
  BRK.B 2017-01-06 163.44 163.80 162.64 163.41  2697027    N
  BRK.B 2017-01-09 163.04 163.25 162.02 162.02  3564674    N
  BRK.B 2017-01-10 162.00 162.74 161.41 161.47  2671259    N
  BRK.B 2017-01-11 161.54 162.45 161.03 162.23  3305859    N
   EBAY 2017-01-03  29.83  30.19  29.64  29.84  7665031    Q
   EBAY 2017-01-04  29.91  30.01  29.51  29.76  9538779    Q
   EBAY 2017-01-05  29.73  30.08  29.61  30.01  9062195    Q
   EBAY 2017-01-06  29.97  31.16  29.78  31.05 13351423    Q
   EBAY 2017-01-09  31.00  31.03  30.60  30.75 10532655    Q
   EBAY 2017-01-10  30.67  30.72  29.84  30.25 13833143    Q
   EBAY 2017-01-11  30.30  30.42  30.01  30.41  8168999    Q

```

Keys that aren't found result in a type-appropriate `nil`.

```
>>> let listings2 = !Listing(["AAPL", "EBAY"], ['Q', 'Q'])

>>> join prices, listings2 on symbol
 symbol       date   open   high    low  close   volume exch
   AAPL 2017-01-03 115.80 116.33 114.76 116.15 28781865    Q
   AAPL 2017-01-04 115.85 116.51 115.75 116.02 21118116    Q
   AAPL 2017-01-05 115.92 116.86 115.81 116.61 22193587    Q
   AAPL 2017-01-06 116.78 118.16 116.47 117.91 31751900    Q
   AAPL 2017-01-09 117.95 119.43 117.94 118.99 33561948    Q
   AAPL 2017-01-10 118.77 119.38 118.30 119.11 24462051    Q
   AAPL 2017-01-11 118.74 119.93 118.60 119.75 27588593    Q
  BRK.B 2017-01-03 164.34 164.71 162.44 163.83  4090967     
  BRK.B 2017-01-04 164.45 164.57 163.00 164.08  3568919     
  BRK.B 2017-01-05 164.06 164.14 162.18 163.30  2982464     
  BRK.B 2017-01-06 163.44 163.80 162.64 163.41  2697027     
  BRK.B 2017-01-09 163.04 163.25 162.02 162.02  3564674     
  BRK.B 2017-01-10 162.00 162.74 161.41 161.47  2671259     
  BRK.B 2017-01-11 161.54 162.45 161.03 162.23  3305859     
   EBAY 2017-01-03  29.83  30.19  29.64  29.84  7665031    Q
   EBAY 2017-01-04  29.91  30.01  29.51  29.76  9538779    Q
   EBAY 2017-01-05  29.73  30.08  29.61  30.01  9062195    Q
   EBAY 2017-01-06  29.97  31.16  29.78  31.05 13351423    Q
   EBAY 2017-01-09  31.00  31.03  30.60  30.75 10532655    Q
   EBAY 2017-01-10  30.67  30.72  29.84  30.25 13833143    Q
   EBAY 2017-01-11  30.30  30.42  30.01  30.41  8168999    Q

```

### Asof Joins

Joins can be `asof`, meaning that, for each row in the left table, we take the last row in the right table whose value is *less than or equal to* the value in the left table. Both tables must be sorted by the value.

```
>>> data Reporting: date: Date, quarter: String end

>>> let reports = !Reporting([Date("2017-01-01"), Date("2017-01-09")], ["Q1", "Q2"])

>>> let p = sort prices by date

>>> join p, reports asof date
 symbol       date   open   high    low  close   volume quarter
   AAPL 2017-01-03 115.80 116.33 114.76 116.15 28781865      Q1
  BRK.B 2017-01-03 164.34 164.71 162.44 163.83  4090967      Q1
   EBAY 2017-01-03  29.83  30.19  29.64  29.84  7665031      Q1
   AAPL 2017-01-04 115.85 116.51 115.75 116.02 21118116      Q1
  BRK.B 2017-01-04 164.45 164.57 163.00 164.08  3568919      Q1
   EBAY 2017-01-04  29.91  30.01  29.51  29.76  9538779      Q1
   AAPL 2017-01-05 115.92 116.86 115.81 116.61 22193587      Q1
  BRK.B 2017-01-05 164.06 164.14 162.18 163.30  2982464      Q1
   EBAY 2017-01-05  29.73  30.08  29.61  30.01  9062195      Q1
   AAPL 2017-01-06 116.78 118.16 116.47 117.91 31751900      Q1
  BRK.B 2017-01-06 163.44 163.80 162.64 163.41  2697027      Q1
   EBAY 2017-01-06  29.97  31.16  29.78  31.05 13351423      Q1
   AAPL 2017-01-09 117.95 119.43 117.94 118.99 33561948      Q2
  BRK.B 2017-01-09 163.04 163.25 162.02 162.02  3564674      Q2
   EBAY 2017-01-09  31.00  31.03  30.60  30.75 10532655      Q2
   AAPL 2017-01-10 118.77 119.38 118.30 119.11 24462051      Q2
  BRK.B 2017-01-10 162.00 162.74 161.41 161.47  2671259      Q2
   EBAY 2017-01-10  30.67  30.72  29.84  30.25 13833143      Q2
   AAPL 2017-01-11 118.74 119.93 118.60 119.75 27588593      Q2
  BRK.B 2017-01-11 161.54 162.45 161.03 162.23  3305859      Q2
   EBAY 2017-01-11  30.30  30.42  30.01  30.41  8168999      Q2

```

A `strict asof` means that we get the last row whose value is *strictly less than*, as opposed to the default *less than or equal to*. This prohibits exact matches, which is needed when the right table's data represents events that have occurred after the left table's event for the same value.

```
>>> join p, reports asof date strict
 symbol       date   open   high    low  close   volume quarter
   AAPL 2017-01-03 115.80 116.33 114.76 116.15 28781865      Q1
  BRK.B 2017-01-03 164.34 164.71 162.44 163.83  4090967      Q1
   EBAY 2017-01-03  29.83  30.19  29.64  29.84  7665031      Q1
   AAPL 2017-01-04 115.85 116.51 115.75 116.02 21118116      Q1
  BRK.B 2017-01-04 164.45 164.57 163.00 164.08  3568919      Q1
   EBAY 2017-01-04  29.91  30.01  29.51  29.76  9538779      Q1
   AAPL 2017-01-05 115.92 116.86 115.81 116.61 22193587      Q1
  BRK.B 2017-01-05 164.06 164.14 162.18 163.30  2982464      Q1
   EBAY 2017-01-05  29.73  30.08  29.61  30.01  9062195      Q1
   AAPL 2017-01-06 116.78 118.16 116.47 117.91 31751900      Q1
  BRK.B 2017-01-06 163.44 163.80 162.64 163.41  2697027      Q1
   EBAY 2017-01-06  29.97  31.16  29.78  31.05 13351423      Q1
   AAPL 2017-01-09 117.95 119.43 117.94 118.99 33561948      Q1
  BRK.B 2017-01-09 163.04 163.25 162.02 162.02  3564674      Q1
   EBAY 2017-01-09  31.00  31.03  30.60  30.75 10532655      Q1
   AAPL 2017-01-10 118.77 119.38 118.30 119.11 24462051      Q2
  BRK.B 2017-01-10 162.00 162.74 161.41 161.47  2671259      Q2
   EBAY 2017-01-10  30.67  30.72  29.84  30.25 13833143      Q2
   AAPL 2017-01-11 118.74 119.93 118.60 119.75 27588593      Q2
  BRK.B 2017-01-11 161.54 162.45 161.03 162.23  3305859      Q2
   EBAY 2017-01-11  30.30  30.42  30.01  30.41  8168999      Q2

```

We can specify a maximum tolerance `within` which the matched key must exist. Anything beyond this is not matched and therefore results in a `nil`.

```
>>> join p, reports asof date within 3d
 symbol       date   open   high    low  close   volume quarter
   AAPL 2017-01-03 115.80 116.33 114.76 116.15 28781865      Q1
  BRK.B 2017-01-03 164.34 164.71 162.44 163.83  4090967      Q1
   EBAY 2017-01-03  29.83  30.19  29.64  29.84  7665031      Q1
   AAPL 2017-01-04 115.85 116.51 115.75 116.02 21118116      Q1
  BRK.B 2017-01-04 164.45 164.57 163.00 164.08  3568919      Q1
   EBAY 2017-01-04  29.91  30.01  29.51  29.76  9538779      Q1
   AAPL 2017-01-05 115.92 116.86 115.81 116.61 22193587        
  BRK.B 2017-01-05 164.06 164.14 162.18 163.30  2982464        
   EBAY 2017-01-05  29.73  30.08  29.61  30.01  9062195        
   AAPL 2017-01-06 116.78 118.16 116.47 117.91 31751900        
  BRK.B 2017-01-06 163.44 163.80 162.64 163.41  2697027        
   EBAY 2017-01-06  29.97  31.16  29.78  31.05 13351423        
   AAPL 2017-01-09 117.95 119.43 117.94 118.99 33561948      Q2
  BRK.B 2017-01-09 163.04 163.25 162.02 162.02  3564674      Q2
   EBAY 2017-01-09  31.00  31.03  30.60  30.75 10532655      Q2
   AAPL 2017-01-10 118.77 119.38 118.30 119.11 24462051      Q2
  BRK.B 2017-01-10 162.00 162.74 161.41 161.47  2671259      Q2
   EBAY 2017-01-10  30.67  30.72  29.84  30.25 13833143      Q2
   AAPL 2017-01-11 118.74 119.93 118.60 119.75 27588593      Q2
  BRK.B 2017-01-11 161.54 162.45 161.03 162.23  3305859      Q2
   EBAY 2017-01-11  30.30  30.42  30.01  30.41  8168999      Q2

```

The default direction is `backward` by default, though `forward` and `nearest` are also allowed.

```
>>> let near_reports = !Reporting([Date("2017-01-01"), Date("2017-01-05"), Date("2017-01-09")], ["Q1", "Q2", "Q3"])

>>> join p, near_reports asof date nearest
 symbol       date   open   high    low  close   volume quarter
   AAPL 2017-01-03 115.80 116.33 114.76 116.15 28781865      Q1
  BRK.B 2017-01-03 164.34 164.71 162.44 163.83  4090967      Q1
   EBAY 2017-01-03  29.83  30.19  29.64  29.84  7665031      Q1
   AAPL 2017-01-04 115.85 116.51 115.75 116.02 21118116      Q2
  BRK.B 2017-01-04 164.45 164.57 163.00 164.08  3568919      Q2
   EBAY 2017-01-04  29.91  30.01  29.51  29.76  9538779      Q2
   AAPL 2017-01-05 115.92 116.86 115.81 116.61 22193587      Q2
  BRK.B 2017-01-05 164.06 164.14 162.18 163.30  2982464      Q2
   EBAY 2017-01-05  29.73  30.08  29.61  30.01  9062195      Q2
   AAPL 2017-01-06 116.78 118.16 116.47 117.91 31751900      Q2
  BRK.B 2017-01-06 163.44 163.80 162.64 163.41  2697027      Q2
   EBAY 2017-01-06  29.97  31.16  29.78  31.05 13351423      Q2
   AAPL 2017-01-09 117.95 119.43 117.94 118.99 33561948      Q3
  BRK.B 2017-01-09 163.04 163.25 162.02 162.02  3564674      Q3
   EBAY 2017-01-09  31.00  31.03  30.60  30.75 10532655      Q3
   AAPL 2017-01-10 118.77 119.38 118.30 119.11 24462051      Q3
  BRK.B 2017-01-10 162.00 162.74 161.41 161.47  2671259      Q3
   EBAY 2017-01-10  30.67  30.72  29.84  30.25 13833143      Q3
   AAPL 2017-01-11 118.74 119.93 118.60 119.75 27588593      Q3
  BRK.B 2017-01-11 161.54 162.45 161.03 162.23  3305859      Q3
   EBAY 2017-01-11  30.30  30.42  30.01  30.41  8168999      Q3

```

### Asof/On Join

We can join both `on` an exact key and `asof` a time.

```
>>> data FullReporting: symbol: String, date: Date, quarter: String end

>>> let full_reports = !FullReporting(["AAPL", "BRK.B", "EBAY", "AAPL", "BRK.B"], [Date("2017-01-01"), Date("2017-01-01"), Date("2017-01-04"), Date("2017-01-06"), Date("2017-01-11")], ["Q1", "Q1", "Q2", "Q2", "Q2"])

>>> full_reports
 symbol       date quarter
   AAPL 2017-01-01      Q1
  BRK.B 2017-01-01      Q1
   EBAY 2017-01-04      Q2
   AAPL 2017-01-06      Q2
  BRK.B 2017-01-11      Q2

>>> join p, full_reports on symbol asof date
 symbol       date   open   high    low  close   volume quarter
   AAPL 2017-01-03 115.80 116.33 114.76 116.15 28781865      Q1
  BRK.B 2017-01-03 164.34 164.71 162.44 163.83  4090967      Q1
   EBAY 2017-01-03  29.83  30.19  29.64  29.84  7665031        
   AAPL 2017-01-04 115.85 116.51 115.75 116.02 21118116      Q1
  BRK.B 2017-01-04 164.45 164.57 163.00 164.08  3568919      Q1
   EBAY 2017-01-04  29.91  30.01  29.51  29.76  9538779      Q2
   AAPL 2017-01-05 115.92 116.86 115.81 116.61 22193587      Q1
  BRK.B 2017-01-05 164.06 164.14 162.18 163.30  2982464      Q1
   EBAY 2017-01-05  29.73  30.08  29.61  30.01  9062195      Q2
   AAPL 2017-01-06 116.78 118.16 116.47 117.91 31751900      Q2
  BRK.B 2017-01-06 163.44 163.80 162.64 163.41  2697027      Q1
   EBAY 2017-01-06  29.97  31.16  29.78  31.05 13351423      Q2
   AAPL 2017-01-09 117.95 119.43 117.94 118.99 33561948      Q2
  BRK.B 2017-01-09 163.04 163.25 162.02 162.02  3564674      Q1
   EBAY 2017-01-09  31.00  31.03  30.60  30.75 10532655      Q2
   AAPL 2017-01-10 118.77 119.38 118.30 119.11 24462051      Q2
  BRK.B 2017-01-10 162.00 162.74 161.41 161.47  2671259      Q1
   EBAY 2017-01-10  30.67  30.72  29.84  30.25 13833143      Q2
   AAPL 2017-01-11 118.74 119.93 118.60 119.75 27588593      Q2
  BRK.B 2017-01-11 161.54 162.45 161.03 162.23  3305859      Q2
   EBAY 2017-01-11  30.30  30.42  30.01  30.41  8168999      Q2

```
