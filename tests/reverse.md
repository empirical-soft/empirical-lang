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

### String

```
>>> reverse("")
""

>>> reverse("Hello")
"olleH"

```

### Scalars -- error

```
>>> reverse(5)
Error: unable to match overloaded function reverse
  candidate: ([Int64]) -> [Int64]
    argument type at position 0 does not match: Int64 vs [Int64]
  candidate: ([Float64]) -> [Float64]
    argument type at position 0 does not match: Int64 vs [Float64]
  candidate: ([Bool]) -> [Bool]
    argument type at position 0 does not match: Int64 vs [Bool]
  ...
  <8 others>

>>> reverse(Person("A", 1))
Error: unable to match overloaded function reverse
  candidate: ([Int64]) -> [Int64]
    argument type at position 0 does not match: Person vs [Int64]
  candidate: ([Float64]) -> [Float64]
    argument type at position 0 does not match: Person vs [Float64]
  candidate: ([Bool]) -> [Bool]
    argument type at position 0 does not match: Person vs [Bool]
  ...
  <8 others>

```
