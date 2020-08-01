These tests cover assignment.

### Setup

```
>>> var x = 1

>>> var xs = [1, 2, 3]

>>> data Person: name: String, age: Int64 end

>>> var p = Person("a", 1)

```

### Scalar

```
>>> x
1

>>> x = 17

>>> x
17

```

### Vector

```
>>> xs
[1, 2, 3]

>>> xs[1] = 13

>>> xs
[1, 13, 3]

```

### User-defined type

```
>>> p
 name age
    a   1

>>> p.name = "b"

>>> p
 name age
    b   1

```
