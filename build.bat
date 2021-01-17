@echo off

rem Copyright 2020 John Pursey
rem
rem Use of this source code is governed by an MIT-style License that can be
rem found in the LICENSE file or at https://opensource.org/licenses/MIT.

setlocal

rem ===========================================================================
pushd "%~dp0"
rem ===========================================================================

set GB_DIR=%~dp0

rem ---------------------------------------------------------------------------
rem Create build directory tree if it doesn't already exist

if not exist build (
	mkdir build
)
cd build

rem ---------------------------------------------------------------------------
rem Build Source

echo ======== CREATING Game Bits Project ========
cmake .. %*

rem ---------------------------------------------------------------------------
rem Build Examples

if not exist examples (
  mkdir examples
)
cd examples

echo ======== CREATING Block World Project ========
if not exist block-world (
  mkdir block-world
)
cd block-world
cmake ../../../examples/block-world %*
cd ..

rem ===========================================================================
rem Back to calling directory
popd
rem ===========================================================================
