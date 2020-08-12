@echo off

if "%1" == "" .\scripts\build.bat

if "%1" == "prod" .\scripts\build.bat prod

if "%1" == "test" .\scripts\test.bat

if "%1" == "deploy" .\scripts\deploy.bat

if "%1" == "clean" rd /q /s build thirdparty\asdl\__pycache__ tests\*.csv
