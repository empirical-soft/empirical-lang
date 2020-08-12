These tests cover compile-time evaluation.

### Multable

```
>>> var x = 7 + 3

>>> let y = x

>>> x = 20

>>> x
20

>>> y
10

```

### Immutable

```
>>> let a = 7 + 3

>>> let b = a

>>> a
10

>>> b
10

```

### Computation mode

```
>>> mode_of(x)
Normal

>>> mode_of(y)
Normal

>>> mode_of(a)
Comptime

>>> mode_of(b)
Comptime

```

### compile()

```
>>> let op = "+"

>>> compile("2" + op + "3")
5

>>> compile("data Person: name: String, age: Int64 end")

>>> Person("a", 1)
 name age
    a   1

>>> compile("func inc(x: Int64): return x + 1 end")

>>> inc(4)
5

```

### Template Functions

```
>>> func foo{x: Int64}(y: Int64): return x + y end

>>> foo{3}(4)
7

>>> foo{a + 23}(27)
60

>>> func add{T: Type}(x: T, y: T): return x + y end

>>> add{Int64}(17, 23)
40

>>> type_of(add{Float64})
<type: (Float64, Float64) -> Float64>

>>> func double_it{T: Type}(a: T): return a + a end

>>> double_it{type_of(x)}(7)
14

>>> func triple_it{T}(a: T): return a + a + a end

>>> triple_it{String}("Hello ")
"Hello Hello Hello "

```

### Template Data

```
>>> data Person{AgeType: Type}: name: String, age: AgeType end

>>> Person{Int64}("a", 13)
 name age
    a  13

>>> Person{Float64}("a", 1.3)
 name age
    a 1.3

>>> !Person{Int64}(["A", "B"], [1, 2])
 name age
    A   1
    B   2

```
