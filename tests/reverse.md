These tests cover `reverse()`.

### Builtin

```
>>> reverse([1, 2, 3])
[3, 2, 1]

>>> reverse(['a', 'b'])
['b', 'a']

```

### Dataframe

```
>>> data Person: name: String, age: Int64 end

>>> reverse(!Person(["A", "B"], [1, 2]))
 name age
    B   2
    A   1

```

### Scalars -- error

```
>>> reverse(5)
Error: Should have invoked internal_reverse() on $0

>>> reverse(Person("A", 1))
 name age
    A   1

```
