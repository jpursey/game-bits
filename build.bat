@echo off

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

if not exist third_party (
	mkdir third_party
)
pushd third_party

echo ======== BUILDING googletest ========
if not exist googletest (
	mkdir googletest
)
pushd googletest
cmake ..\..\..\third_party\googletest -Dgtest_force_shared_crt=ON %*
cmake --build . --config Release
cmake --build . --config Debug
cmake --build . --config MinSizeRel
cmake --build . --config RelWithDebInfo
popd

echo ======== BUILDING glog ========
if not exist glog (
  mkdir glog
)
pushd glog
cmake ..\..\..\third_party\glog %*
cmake --build . --config Release
cmake --build . --config Debug
cmake --build . --config MinSizeRel
cmake --build . --config RelWithDebInfo
popd

rem Back to build/ directory
popd

rem ---------------------------------------------------------------------------
rem Build Source

echo ======== BUILDING Source ========
cmake .. %*

rem ===========================================================================
rem Back to calling directory
popd
rem ===========================================================================
