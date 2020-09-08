This test is for a bug that assigns global variables to local registers after an error. See [GH 95](https://github.com/empirical-soft/empirical-lang/issues/95).

### Preliminary setup

```
>>> let trades = load("wrong.csv")
Error: wrong.csv: No such file or directory

```

### Broken global

```
>>> var x = 7

>>> func foo(y: Int64) = x + y

>>> foo(4)
11

```
