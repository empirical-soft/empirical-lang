## Regression tests for utils

These regression tests cover the functionality from the `utils` headers. To add a new test, simply add a rule to `CMakeLists.txt`.

All of these tests rely on the macros in `test.hpp`:

- `TEST(x, y)`: ensure values are equal
- `TEST_NIL(x)`: ensure value is missing
- `P(...)`: wrapper for handling commas

Any test file should look like this:

```
#include "test.hpp"

// include necessary utils headers here...

int main() {
  main_ret = 0;
  
  // invoke TEST() and TEST_NIL() here...

  return main_ret;
}
```
