#!/bin/bash

# Run regression tests

cd build
ctest -j8 --output-on-failure
