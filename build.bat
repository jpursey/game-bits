@echo off
setlocal

rem ===========================================================================
pushd "%~dp0"
rem ===========================================================================

rem ---------------------------------------------------------------------------
rem Create build directory tree if it doesn't already exist

if not exist build (
	mkdir build
)
cd build

rem ---------------------------------------------------------------------------
rem Build third_party Source

set GB_DIR=%~dp0
call ..\third_party\build.bat %*

rem ---------------------------------------------------------------------------
rem Build Source

echo ======== BUILDING Source ========
cmake .. %*

rem ===========================================================================
rem Back to calling directory
popd
rem ===========================================================================
