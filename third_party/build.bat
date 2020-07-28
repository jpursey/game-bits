rem Copyright 2020 John Pursey
rem
rem Use of this source code is governed by an MIT-style License that can be
rem found in the LICENSE file or at https://opensource.org/licenses/MIT.

if "%GB_DIR%" == "" (
  echo GB_DIR is not defined!
  goto :EOF
)

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
cmake "%GB_DIR%\third_party\googletest" -Dgtest_force_shared_crt=ON %*
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
cmake "%GB_DIR%\third_party\glog" %*
cmake --build . --config Release
cmake --build . --config Debug
cmake --build . --config MinSizeRel
cmake --build . --config RelWithDebInfo
popd

rem Back to build/ directory
popd
