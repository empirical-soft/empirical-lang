rem Run regression tests

@echo off
cd build
if exist "MinSizeRel" (set config=MinSizeRel) else (set config=Debug)
ctest -C %config% -j8 --output-on-failure
