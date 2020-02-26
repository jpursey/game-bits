if "%GBITS_DIR%" == "" (
  echo GBITS_DIR is not defined!
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
cmake "%GBITS_DIR%\third_party\googletest" -Dgtest_force_shared_crt=ON %*
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
cmake "%GBITS_DIR%\third_party\glog" %*
cmake --build . --config Release
cmake --build . --config Debug
cmake --build . --config MinSizeRel
cmake --build . --config RelWithDebInfo
popd

rem Back to build/ directory
popd
