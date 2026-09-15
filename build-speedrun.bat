@echo off
setlocal

:: CTR Speedrun client build (Windows). Same toolchain as build.bat, plus
:: CTR_SPEEDRUN=ON. Requires MSYS2 MinGW32 tools in PATH.

where i686-w64-mingw32-gcc >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ERROR: i686-w64-mingw32-gcc not found in PATH
    echo Add C:\msys64\mingw32\bin to your PATH, then retry.
    exit /b 1
)

cmake -S . -B build-speedrun -G "MinGW Makefiles" -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ -DCMAKE_MAKE_PROGRAM=mingw32-make -DCMAKE_BUILD_TYPE=Release "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" -DCTR_SPEEDRUN=ON
if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake configure failed
    exit /b 1
)

cmake --build build-speedrun -j
if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo Build succeeded: build-speedrun\ctr_native.exe
