## Regression tests for VVM

These regressions tests (`*.vvm`) work by comparing the stdout results of each file to the expected output, designated by `;;`. For example:

```
@1 = "Hello"
write @1
;;Hello
```

The tests are run by `test_vvm.sh`, which indicates an error if outputs don't match. All `*.vvm` programs are run; just put a new file in this directory to include it as a test.
