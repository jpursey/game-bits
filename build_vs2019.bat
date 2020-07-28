@echo off

rem Copyright 2020 John Pursey
rem
rem Use of this source code is governed by an MIT-style License that can be
rem found in the LICENSE file or at https://opensource.org/licenses/MIT.

rem ===========================================================================
pushd "%~dp0"
rem ===========================================================================

build.bat -G "Visual Studio 16 2019"

rem ===========================================================================
popd
rem ===========================================================================
