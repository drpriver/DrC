@echo off
where clang >nul 2>nul && (clang build.c -o build.exe && build.exe %* -b Bin) && exit /b
where gcc >nul 2>nul && (gcc build.c -o build.exe && build.exe %* -b Bin) && exit /b
where cl >nul 2>nul && (cl build.c /Fe:build.exe && build.exe %* -b Bin) && exit /b
echo No C compiler found
exit /b 1
