## Regression tests for Empirical

These regressions tests work by comparing the stdout results of each file to the expected output, designated by `##`. For example:

```
let x = 1+2*3
print(String(x))
##7
```

The tests are run by `test_emp.sh`, which indicates an error if outputs don't match. Since all `*.emp` programs are run, there is no need to amend the script for any new tests.

----

## Markdown-based tests

Additional tests are in the form of Markdown files (`*.md`). These must be added manually to the `CMakeLists.txt` for now.
