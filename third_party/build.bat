@echo off

REM ===========================================================================
pushd "%~dp0"
REM ===========================================================================

REM ---------------------------------------------------------------------------
REM Create build directory tree if it doesn't already exist

if not exist build (
	mkdir build
)
cd build

echo ======== BUILDING googletest ========
if not exist googletest (
	mkdir googletest
)
pushd googletest
cmake ..\..\googletest -Dgtest_force_shared_crt=ON %*
cmake --build . --config Release
cmake --build . --config Debug
cmake --build . --config MinSizeRel
cmake --build . --config RelWithDebInfo
popd

echo ======== BUILDING abseil-cpp ========
if not exist abseil-cpp (
	mkdir abseil-cpp
)
pushd abseil-cpp
cmake ..\..\abseil-cpp %*
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
cmake ..\..\glog %*
cmake --build . --config Release
cmake --build . --config Debug
cmake --build . --config MinSizeRel
cmake --build . --config RelWithDebInfo
popd

REM ===========================================================================
popd
REM ===========================================================================
