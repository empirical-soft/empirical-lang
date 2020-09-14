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

### String

```
>>> len("")
0

>>> len("Hello")
5

```

### Scalars -- error

```
>>> len(5)
Error: unable to match overloaded function len
  candidate: ([Int64]) -> Int64
    argument type at position 0 does not match: Int64 vs [Int64]
  candidate: ([Float64]) -> Int64
    argument type at position 0 does not match: Int64 vs [Float64]
  candidate: ([Bool]) -> Int64
    argument type at position 0 does not match: Int64 vs [Bool]
  ...
  <8 others>

>>> len(Person("A", 1))
Error: unable to match overloaded function len
  candidate: ([Int64]) -> Int64
    argument type at position 0 does not match: Person vs [Int64]
  candidate: ([Float64]) -> Int64
    argument type at position 0 does not match: Person vs [Float64]
  candidate: ([Bool]) -> Int64
    argument type at position 0 does not match: Person vs [Bool]
  ...
  <8 others>

```
