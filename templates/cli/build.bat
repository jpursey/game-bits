@echo off
setlocal

rem This script depends on the GB_DIR environment variable.
if "%GB_DIR%" == "" (
  echo GB_DIR is not defined!
  goto :EOF
)

rem Create a local build directory, and build CMake output there.
echo ======== CREATING Main Project ========
pushd "%~dp0"
if not exist build (
  mkdir build
)
cd build
cmake .. %*
popd
