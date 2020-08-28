@echo off

for /r %%i in (*.frag) do (
  echo Compiling %%i...
  glslc %%i -o %%i.spv
)

for /r %%i in (*.vert) do (
  echo Compiling %%i...
  glslc %%i -o %%i.spv
)
