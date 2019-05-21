## Regression tests for VVM

These regressions tests work by comparing the stdout results of each file to the expected output, designated by `;;`. For example:

```
@1 = "Hello"
write @1
;;Hello
```

The tests are run by `test_vvm.sh`, which indicates an error if outputs don't match. Since all `*.vvm` programs are run, there is no need to amend the script for any new tests.
