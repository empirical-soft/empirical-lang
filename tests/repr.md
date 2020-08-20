These tests cover repr.

### Scalar

```
>>> 15
15

>>> 15.5
15.5

>>> true
true

>>> "Hello"
"Hello"

>>> 'c'
'c'

>>> Timestamp("2020-08-20 22:17:32.529615")
Timestamp("2020-08-20 22:17:32.529615")

>>> 1s
Timedelta("00:00:01")

>>> Time("22:17:32.529615")
Time("22:17:32.529615")

>>> Date("2020-08-20")
Date("2020-08-20")

```

### Vector

```
>>> [15, 16]
[15, 16]

>>> [15.5, 16.5]
[15.5, 16.5]

>>> [true, false]
[true, false]

>>> ["Hello", "World"]
["Hello", "World"]

>>> ['c', 'd']
['c', 'd']

>>> [Timestamp("2020-08-20 22:17:32.529615"), Timestamp("2020-08-21 22:17:33.529615")]
[Timestamp("2020-08-20 22:17:32.529615"), Timestamp("2020-08-21 22:17:33.529615")]

>>> [1s, 5ms]
[Timedelta("00:00:01"), Timedelta("00:00:00.005")]

>>> [Time("22:17:32.529615"), Time("22:17:33.529615")]
[Time("22:17:32.529615"), Time("22:17:33.529615")]

>>> [Date("2020-08-20"), Date("2020-08-21")]
[Date("2020-08-20"), Date("2020-08-21")]

```

### UDT

```
>>> data Person: name: String, age: Int64 end

>>> Person("AA", 11)
 name age
   AA  11

>>> !Person(["AA", "BB"], [11, 22])
 name age
   AA  11
   BB  22

```
