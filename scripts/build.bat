rem Build Empirical, possibly with the "prod" directive

@echo off
setlocal

if "%1" == "prod" (set config=MinSizeRel) else (set config=Debug)

mkdir build
cd build
cmake ..
cmake --build . --config %config% -- /m:8
