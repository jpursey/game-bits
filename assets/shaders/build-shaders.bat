@echo off

rem Compiles the shaders in this folder and below, wherever it is run from.
pushd "%~dp0"

for /r %%i in (*.frag) do (
  echo Compiling %%i...
  glslc %%i -o %%i.spv
)

for /r %%i in (*.vert) do (
  echo Compiling %%i...
  glslc %%i -o %%i.spv
)

popd
