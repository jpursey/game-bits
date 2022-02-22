@echo off

rem Copyright 2022 John Pursey
rem
rem Use of this source code is governed by an MIT-style License that can be
rem found in the LICENSE file or at https://opensource.org/licenses/MIT.

rem ===========================================================================
pushd "%~dp0"
rem ===========================================================================

build.bat -G "Visual Studio 17 2022"

rem ===========================================================================
popd
rem ===========================================================================
