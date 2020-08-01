## Regression tests for Empirical

These regressions tests (`*.emp`) work by comparing the stdout results of each file to the expected output, designated by `##`. For example:

```
let x = 1+2*3
print(String(x))
##7
```

The tests are run by `test_emp.sh`, which indicates an error if outputs don't match. All `*.emp` programs are run; just put a new file in this directory to include it as a test.

----

## Markdown-based tests

Additional tests are in the form of Markdown files (`*.md`). They compare the returned value with the expected. For example:

```
>>> 3 + 7
10

```

All `*.md` files are run; just put a new file in this directory to include it as a test.
