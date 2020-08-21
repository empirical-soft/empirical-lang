These tests cover `len()`.

### Builtin

```
>>> len([1, 2, 3])
3

>>> len(['a', 'b'])
2

```

### Dataframe

```
>>> data Person: name: String, age: Int64 end

>>> len(!Person(["A", "B"], [1, 2]))
2

```

### Scalars -- error

```
>>> len(5)
Error: Index out of bounds

>>> len(Person("A", 1))
Error: Index out of bounds

```
