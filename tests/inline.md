These tests cover inlining

```
>>> func foo(x) => x + 20

>>> foo(9)
29

>>> func foo2{x: Int64}(y: Int64) => x - y

>>> foo2{15}(30)
-15

```
