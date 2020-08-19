## Launching

Launch Empirical from the command line to get the REPL.

```skip
$ path/to/empirical
Empirical version 0.6.0
Copyright (C) 2019--2020 Empirical Software Solutions, LLC

>>>

```

Alternatively, include a file name to run it.

```skip
$ path/to/empirical file_to_run.emp

```

There are some advanced options on the command line to see the internal state of the compiler. Get the `--help` for the full list.

```skip
$ path/to/empirical --help

```

### Magic commands

The REPL has some magic commands that may make development easier. For example, we can time an expression:

```skip
>>> \t let p = load("prices.csv")

 1ms

```

We can also load an external Empirical file.

```skip
>>> \l my_code.emp

```

To see all available magic commands, just ask for `\help`.

```skip
>>> \help

```

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
Error: unable to determine type for user_age

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

The type system is *static* and *strict*; this prevents common errors.

```
>>> x + "5"
Error: unable to match overloaded function +
  candidate: (Int64, Int64) -> Int64
    argument type at position 1 does not match: String vs Int64
  candidate: (Float64, Float64) -> Float64
    argument type at position 0 does not match: Int64 vs Float64
  candidate: (Int64, Float64) -> Float64
    argument type at position 1 does not match: String vs Float64
  ...
  <53 others>

```

A value can be cast to a desired type.

```
>>> x + Int64("5")
42

```

If a cast is invalid, then we will have a `nil` (integers) or `nan` (floating point). The *missing data* value is propagated by operators.

```
>>> x + Int64("5b")
nil

```

### User-defined types

Users can define their own values.

```
>>> data Person: name: String, age: Int64 end

>>> var p = Person("Alice", 37)

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

We can define a *type cast* if desired.

```
>>> func String(p: Person) = p.name + " is " + String(p.age) + " years old"

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

User-defined types can accept *templates*.

```
>>> data Person2{AgeType}: name: String, age: AgeType end

>>> Person2{Int64}("A", 1)
 name age
    A   1

>>> !Person2{Float64}(["A", "B"], [1.1, 1.2])
 name age
    A 1.1
    B 1.2

```

The above examples are in *statement syntax*. Types can be defined with *expression syntax*.

```
>>> data I = Int64

>>> var i: I

>>> i = 17

>>> data Person3 = {name: String, age: Int64}

```

Templates and expression syntax can be combined for a *type provider*. This allows for programmatically determining a type.

```
>>> data Provider{f: String} = compile(f)

>>> let s = "{name: String, age: Int64}"

>>> var obj: Provider{s}

```

We can always recall the type of an expression.

```
>>> type_of(x)
<type: Int64>

>>> type_of(i)
<type: Int64>

>>> type_of(x > 7)
<type: Bool>

>>> var y: type_of(x)

>>> y = 7

>>> type_of(Int64)
<type: Kind(Int64)>

>>> Int64
<type: Int64>

>>> type_of(p)
<type: Person>

>>> Person
<type: Person>

>>> Person2
<template>

>>> Person3
<type: Person3>

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

The type can be recalled.

```
>>> type_of(xs)
<type: [Float64]>

>>> [Float64]
<type: [Float64]>

```

## Functions

Functions apply to a list of arguments.

```
>>> func add(x, y) = x + y

>>> add(3, 7)
10

>>> add("A", "B")
"AB"

```

The above is an example of *expression syntax*, where the function is defined as a single expression. Functions can also be defined with *statement syntax* with an explicit `return`.

```
>>> func mult(x, y): return x * y end

>>> mult(3, 7)
21

>>> mult(0.1, 0.9)
0.09

```

The argument types are determined from the caller, but can be listed explicitly.

```
>>> func add2(x: Int64, y: Int64): return x + y end

```

Functions can be overloaded by parameter type.

```
>>> func add2(x: Bool, y: Bool): return x or y end

>>> add2(true, false)
true

>>> add2(1, 0)
1

```

An error occurs if types don't match during a function call.

```
>>> add2(3.4, 5.6)
Error: unable to match overloaded function add2
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

>>> func (+)(p: Point, n: Int64) = Point(p.x + n, p.y + n)

>>> Point(5, 7) + 12
  x  y
 17 19

```

User-defined literals can be defined by prepending `suffix` to any function name.

```
>>> func suffix_w(x: Int64) = x * 3

>>> 7_w
21

>>> 0xFF_w
765

>>> func suffix_z(x: Float64): return 3.0 * x end

>>> 1.2e4_z
36000.0

```

The return type is also inferred, but may be listed explicitly.

```
>>> func add3(x: Int64, y: Int64, z: Int64) -> Int64: return add2(add2(x, y), z) end

>>> add3(4, 5, 6)
15

```

Functions can, of course, be recursive.

```
>>> func fac(x: Int64): if x == 0: return 1 else: return x * fac(x - 1) end end

>>> fac(5)
120

```

### Metaprogramming

Functions can take templates.

```
>>> func mult2{T}(x: T, y: T) = x * y

>>> mult2{Int64}(4, 6)
24

>>> mult2{Float64}(4.0, 6.0)
24.0

```

The template parameter is a `Type` by default, but value parameters are also permitted.

```
>>> func inc{i: Int64}(x: Int64) = x + i

>>> inc{1}(7)
8

>>> inc{10}(7)
17

```

A macro is possible by prepending a dollar sign to a parameter name. As with templates, the caller must provide a *comptime literal* (a simple value, such as a `String` or `Int64`, that can be derived at compile time).

```
>>> func inc2($ i: Int64, x: Int64) = x + i

>>> inc2(7, 8)
15

```

Empirical's *compile-time function evaluation* (CTFE) will automatically determine the result of an expression ahead of time if possible.

```
>>> let v = 100 - 88

>>> inc2(v / 3, 21)
25

```

A mutable variable is not permitted in CTFE because its value can change. (IO-derived values are also prohibited because their results cannot be determined at compile time.)

```
>>> var u = 100

>>> inc2(u, 50)
Error: macro parameter i requires a comptime literal

```

A function can be inlined, meaning that the function body is pasted into the caller's location. This can speed-up small expressions.

```
>>> func triple(x: Int64) => x + x + x

>>> triple(7)
21

```

A function's type information is available as well.

```
>>> add
<generic func>

>>> type_of(add)
<type: (_, _) -> _>

>>> add3
<func: add3>

>>> type_of(add3)
<type: (Int64, Int64, Int64) -> Int64>

>>> (+)
<func>

>>> type_of(sum)
<type: overloaded>

>>> inc
<template>

>>> inc2
<macro>

>>> type_of(inc{1})
<type: (Int64) -> Int64>

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

*All sample CSV files are available to [download here](https://github.com/empirical-soft/replit) or [use on repl.it](https://repl.it/github/empirical-soft/replit).*

Empirical has statically typed Dataframes. The types can be inferred by `load()` if the parameter is a comptime literal.

```
>>> let trades = load("trades.csv"), quotes = load("quotes.csv"), events = load("events.csv")

>>> trades
 symbol                  timestamp    price size
   AAPL 2019-05-01 09:30:00.578802 210.5200  780
   AAPL 2019-05-01 09:30:00.580485 210.8100  390
    BAC 2019-05-01 09:30:00.629205  30.2500  510
    CVX 2019-05-01 09:30:00.944122 117.8000 5860
   AAPL 2019-05-01 09:30:01.002405 211.1300  320
   AAPL 2019-05-01 09:30:01.066917 211.1186  310
   AAPL 2019-05-01 09:30:01.118968 211.0000  730
    BAC 2019-05-01 09:30:01.186416  30.2450  380
    CVX 2019-05-01 09:30:01.639577 118.2550 2880
    BAC 2019-05-01 09:30:01.867638  30.2450  260
   AAPL 2019-05-01 09:30:02.065535 211.1800  260
    BAC 2019-05-01 09:30:02.118224  30.2600  300
    CVX 2019-05-01 09:30:02.260710 118.3100 1450
    BAC 2019-05-01 09:30:02.379882  30.2650  300
   AAPL 2019-05-01 09:30:02.422211 211.3300  270
    CVX 2019-05-01 09:30:02.439735 118.2900  760
    CVX 2019-05-01 09:30:02.869668 118.2700  980
    BAC 2019-05-01 09:30:02.987527  30.2350  220
   AAPL 2019-05-01 09:30:03.057945 211.4425  300
    CVX 2019-05-01 09:30:03.363338 118.5100  990
    ...                        ...      ...  ...

>>> columns(trades)
symbol: String
timestamp: Timestamp
price: Float64
size: Int64

```

Queries are builtin.

```
>>> from trades select where symbol == "AAPL" and size > 1000
 symbol                  timestamp    price size
   AAPL 2019-05-01 09:37:45.647850 205.0600 1010
   AAPL 2019-05-01 09:38:24.754932 204.9200 2010
   AAPL 2019-05-01 09:42:57.450065 203.7332 1130

```

We can also perform aggregations.

```
>>> from trades select volume = sum(size) by symbol
 symbol volume
   AAPL 135760
    BAC 223590
    CVX 507580

```

Aggregations can take arbitrary expressions, including user-defined functions. Here is an example of a volume-weighted average price (VWAP):

```
>>> func wavg(ws, vs) = sum(ws * vs) / sum(ws)

>>> from trades select vwap = wavg(size, price) by symbol, bar(timestamp, 5m)
 symbol           timestamp       vwap
   AAPL 2019-05-01 09:30:00 210.305724
    BAC 2019-05-01 09:30:00  30.483875
    CVX 2019-05-01 09:30:00 119.427733
   AAPL 2019-05-01 09:35:00 202.972440
    BAC 2019-05-01 09:35:00  30.848397
    CVX 2019-05-01 09:35:00 119.431601
   AAPL 2019-05-01 09:40:00 204.671388
    BAC 2019-05-01 09:40:00  30.217362
    CVX 2019-05-01 09:40:00 117.224763
   AAPL 2019-05-01 09:45:00 206.494583
    BAC 2019-05-01 09:45:00  30.070924
    CVX 2019-05-01 09:45:00 118.073644

```

We can sort a Dataframe by a column or an expression. This example sorts by the bid-ask spread:

```
>>> sort quotes by (ask - bid) / bid
 symbol                  timestamp      bid      ask
    BAC 2019-05-01 09:32:46.313487  30.5650  30.5650
    BAC 2019-05-01 09:32:53.738446  30.6124  30.6124
    BAC 2019-05-01 09:39:24.459415  31.0600  31.0600
   AAPL 2019-05-01 09:45:51.931597 206.9400 206.9500
   AAPL 2019-05-01 09:43:59.903292 206.3200 206.3300
    BAC 2019-05-01 09:32:50.369746  30.6400  30.6417
    CVX 2019-05-01 09:32:57.242072 119.7732 119.7800
   AAPL 2019-05-01 09:38:18.980026 205.1100 205.1222
   AAPL 2019-05-01 09:38:19.978890 205.1100 205.1251
    CVX 2019-05-01 09:37:59.439853 117.5700 117.5800
    CVX 2019-05-01 09:37:15.725633 117.5500 117.5600
    CVX 2019-05-01 09:44:08.411541 117.5500 117.5600
    CVX 2019-05-01 09:37:13.676526 117.5000 117.5100
   AAPL 2019-05-01 09:31:52.241969 214.0800 214.1000
    CVX 2019-05-01 09:37:46.188810 117.8189 117.8300
   AAPL 2019-05-01 09:44:02.188362 206.1700 206.1900
   AAPL 2019-05-01 09:44:05.553974 205.8600 205.8800
   AAPL 2019-05-01 09:37:25.351114 204.9800 205.0000
   AAPL 2019-05-01 09:36:54.176575 204.9100 204.9300
   AAPL 2019-05-01 09:41:08.041997 203.9600 203.9800
    ...                        ...      ...      ...

```

Dataframes can join on one or more columns. They can also join *as of* a column: for every row in the left table, get the last row in the right table whose timestamp is less than or equal to the timestamp in the left.

```
>>> join trades, quotes on symbol asof timestamp
 symbol                  timestamp    price size      bid    ask
   AAPL 2019-05-01 09:30:00.578802 210.5200  780 210.8000 211.15
   AAPL 2019-05-01 09:30:00.580485 210.8100  390 210.8000 211.15
    BAC 2019-05-01 09:30:00.629205  30.2500  510  30.2400  30.27
    CVX 2019-05-01 09:30:00.944122 117.8000 5860 117.7600 118.34
   AAPL 2019-05-01 09:30:01.002405 211.1300  320 210.8000 211.15
   AAPL 2019-05-01 09:30:01.066917 211.1186  310 210.8000 211.15
   AAPL 2019-05-01 09:30:01.118968 211.0000  730 210.8000 211.15
    BAC 2019-05-01 09:30:01.186416  30.2450  380  30.2400  30.27
    CVX 2019-05-01 09:30:01.639577 118.2550 2880 118.2600 118.37
    BAC 2019-05-01 09:30:01.867638  30.2450  260  30.2300  30.26
   AAPL 2019-05-01 09:30:02.065535 211.1800  260 211.1500 211.40
    BAC 2019-05-01 09:30:02.118224  30.2600  300  30.2300  30.26
    CVX 2019-05-01 09:30:02.260710 118.3100 1450 118.2600 118.54
    BAC 2019-05-01 09:30:02.379882  30.2650  300  30.2300  30.26
   AAPL 2019-05-01 09:30:02.422211 211.3300  270 211.2433 211.61
    CVX 2019-05-01 09:30:02.439735 118.2900  760 118.2600 118.54
    CVX 2019-05-01 09:30:02.869668 118.2700  980 118.4800 118.58
    BAC 2019-05-01 09:30:02.987527  30.2350  220  30.2300  30.26
   AAPL 2019-05-01 09:30:03.057945 211.4425  300 211.2433 211.61
    CVX 2019-05-01 09:30:03.363338 118.5100  990 118.4100 118.48
    ...                        ...      ...  ...      ...    ...

```

Asof joins can take parameters, such as changing direction or bounding the search.

```
>>> join trades, events on symbol asof timestamp nearest within 3s
 symbol                  timestamp    price size code
   AAPL 2019-05-01 09:30:00.578802 210.5200  780     
   AAPL 2019-05-01 09:30:00.580485 210.8100  390     
    BAC 2019-05-01 09:30:00.629205  30.2500  510     
    CVX 2019-05-01 09:30:00.944122 117.8000 5860   a1
   AAPL 2019-05-01 09:30:01.002405 211.1300  320     
   AAPL 2019-05-01 09:30:01.066917 211.1186  310     
   AAPL 2019-05-01 09:30:01.118968 211.0000  730     
    BAC 2019-05-01 09:30:01.186416  30.2450  380   e3
    CVX 2019-05-01 09:30:01.639577 118.2550 2880   a1
    BAC 2019-05-01 09:30:01.867638  30.2450  260   e3
   AAPL 2019-05-01 09:30:02.065535 211.1800  260     
    BAC 2019-05-01 09:30:02.118224  30.2600  300   e3
    CVX 2019-05-01 09:30:02.260710 118.3100 1450   a1
    BAC 2019-05-01 09:30:02.379882  30.2650  300   e3
   AAPL 2019-05-01 09:30:02.422211 211.3300  270     
    CVX 2019-05-01 09:30:02.439735 118.2900  760   a1
    CVX 2019-05-01 09:30:02.869668 118.2700  980   a1
    BAC 2019-05-01 09:30:02.987527  30.2350  220   e3
   AAPL 2019-05-01 09:30:03.057945 211.4425  300   f7
    CVX 2019-05-01 09:30:03.363338 118.5100  990   a1
    ...                        ...      ...  ...  ...

```

## Scripting

Empirical scripts get command-line arguments as `argv`. Since the argument values are not known at compile time, and because Empirical is a statically typed language, users cannot call `load()`. Instead, users must supply an explicit type to the templated function `csv_load{}()`. (Fortunately, the type definition can be seen in the REPL ahead-of-time with `columns()`.) Results can be saved with `store()`.


```skip
$ cat aggregate.emp
#!path/to/empirical

# single price row
data Price:
  symbol: String,
  date: Date,
  open: Float64,
  high: Float64,
  low: Float64,
  close: Float64,
  volume: Int64
end

# argv[0] is the script name
if len(argv) != 2:
  print("Missing path to CSV file")
  exit(1)
end

# aggregate volumes from the price file
let prices = csv_load{Price}(argv[1])
let v = from prices select sum(volume) by symbol
store(v, "volumes.csv")
```

If execute privileges are given to the script (`chmod a+x`), then simply run the script on the command line.

```skip
$ ./aggregate.emp
Missing path to CSV file

$ ./aggregate.emp prices.csv

$ cat volumes.csv
symbol,volume
AAPL,277096071
BRK.B,33905036
EBAY,95312664
```
