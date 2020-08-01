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
