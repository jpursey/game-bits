rem Copyright 2020 John Pursey
rem
rem Use of this source code is governed by an MIT-style License that can be
rem found in the LICENSE file or at https://opensource.org/licenses/MIT.

if "%GB_DIR%" == "" (
  echo GB_DIR is not defined!
  goto :EOF
)

goto :BuildAll

rem ---------------------------------------------------------------------------
rem Subroutines

:CMakeBuild
if "%GB_BUILD_TYPE%" == "" (
  cmake --build . --config Release
  cmake --build . --config Debug
  cmake --build . --config MinSizeRel
  cmake --build . --config RelWithDebInfo
)
if "%GB_BUILD_TYPE%" == "Release" (
  cmake --build . --config Release
)
if "%GB_BUILD_TYPE%" == "Debug" (
  cmake --build . --config Debug
)
if "%GB_BUILD_TYPE%" == "MinSizeRel" (
  cmake --build . --config MinSizeRel
)
if "%GB_BUILD_TYPE%" == "RelWithDebInfo" (
  cmake --build . --config RelWithDebInfo
)
exit /b

rem ---------------------------------------------------------------------------
rem Build third_party Source

:BuildAll

if not exist third_party (
  mkdir third_party
)
pushd third_party

echo ======== BUILDING googletest ========
if not exist googletest (
  mkdir googletest
)
pushd googletest
cmake "%GB_DIR%\third_party\googletest" -DINSTALL_GTEST=OFF -Dgtest_force_shared_crt=ON %*
call :CMakeBuild
popd

echo ======== BUILDING glog ========
if not exist glog (
  mkdir glog
)
pushd glog
cmake "%GB_DIR%\third_party\glog" -DWITH_GFLAGS=OFF %*
call :CMakeBuild
popd

echo ======== BUILDING flatbuffers ========
if not exist flatbuffers (
  mkdir flatbuffers
)
pushd flatbuffers
cmake "%GB_DIR%\third_party\flatbuffers" -DFLATBUFFERS_BUILD_TESTS=OFF -DFLATBUFFERS_INSTALL=OFF %*
call :CMakeBuild
popd

rem Back to build/ directory
popd
