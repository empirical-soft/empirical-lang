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
