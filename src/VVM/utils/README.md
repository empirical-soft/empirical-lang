This directory contains utilities for VVM. We wish to separate the VVM-only code (like the assembler and interpreter) from more general-purpose C++ code.

The routines are fairly diverse:

 - `nil.hpp`: represents missing data
 - `conversion.hpp`: convert between types, particularly to and from strings
 - `timestamp.hpp`/`timestamp.cpp`: defines timestamp and related types
 - `csv_infer.hpp`/`csv_infer.cpp`: determines type from a CSV
 - `terminal.hpp`: returns the size of the user's console
 - `timer.hpp`: routines for performance evaluation

There are no generated files for this level of the infrastructure.
