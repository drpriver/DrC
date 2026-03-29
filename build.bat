@echo off
if "%1"=="cl" (shift & goto use_cl)
if "%1"=="clang" (shift & goto use_clang)
if "%1"=="clang-cl" (shift & goto use_clang_cl)
goto auto

:use_cl
cl /nologo /std:c11 /Zc:preprocessor /wd5105 build.c /Fe:build.exe && build.exe %* -b Bin
exit /b

:use_clang
clang -march=native build.c -o build.exe && build.exe %* -b Bin
exit /b

:use_clang_cl
clang-cl -march=native build.c -o build.exe && build.exe %* -b Bin
exit /b

:auto
where cl >nul 2>nul && goto use_cl
where clang >nul 2>nul && goto use_clang
echo No C compiler found
exit /b 1
