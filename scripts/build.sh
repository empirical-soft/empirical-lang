#!/bin/bash

# Build Empirical, possibly with "prod" or "infer" directives

infer=
prod=

while [ "$1" != "" ]; do
  case $1 in
    prod )
      prod="-DCMAKE_BUILD_TYPE=MinSizeRel"
      ;;
    infer )
      infer="infer run --"
      ;;
  esac
  shift
done

mkdir -p build && cd build
cmake $prod ..
$infer cmake --build . -- -j8
