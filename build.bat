@echo off
where cl >nul 2>nul && (cl /nologo /std:c11 /Zc:preprocessor /wd5105 build.c /Fe:build.exe && build.exe %* -b Bin) && exit /b
where clang >nul 2>nul && (clang build.c -o build.exe && build.exe %* -b Bin) && exit /b
echo No C compiler found
exit /b 1
