@echo off
setlocal

echo Building TinyTIFF for both x86 and x64...

:: Set Visual Studio environment - adjust path if needed
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86

:: Build 32-bit version
echo.
echo Building 32-bit version...
cd /d "%~dp0TinyTIFF-4.0.1.0"
if not exist build32 mkdir build32
cd build32
cmake -G "Visual Studio 17 2022" -A Win32 -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DTinyTIFF_BUILD_TESTS=OFF -DTinyTIFF_BUILD_DECORATE_LIBNAMES_WITH_BUILDTYPE=OFF -DCMAKE_CXX_FLAGS_RELEASE="/MT /O2 /Ob2 /DNDEBUG" -DCMAKE_C_FLAGS_RELEASE="/MT /O2 /Ob2 /DNDEBUG" ..
cmake --build . --config Release
if errorlevel 1 goto error

:: Build 64-bit version
echo.
echo Building 64-bit version...
cd ..
if not exist build64 mkdir build64
cd build64
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DTinyTIFF_BUILD_TESTS=OFF -DTinyTIFF_BUILD_DECORATE_LIBNAMES_WITH_BUILDTYPE=OFF -DCMAKE_CXX_FLAGS_RELEASE="/MT /O2 /Ob2 /DNDEBUG" -DCMAKE_C_FLAGS_RELEASE="/MT /O2 /Ob2 /DNDEBUG" ..
cmake --build . --config Release
if errorlevel 1 goto error

:: Copy built libraries to parent directory
echo.
echo Copying libraries...
cd ..
copy /Y build32\src\Release\TinyTIFF.lib ..\TinyTIFF_x86.lib
copy /Y build64\src\Release\TinyTIFF.lib ..\TinyTIFF_x64.lib

:: Copy headers
echo.
echo Copying headers...
copy /Y src\*.h ..\.
copy /Y build64\*.h ..\.

echo.
echo Build completed successfully!
echo Libraries created:
echo   - TinyTIFF_x86.lib (32-bit)
echo   - TinyTIFF_x64.lib (64-bit)
goto end

:error
echo.
echo Build failed!
exit /b 1

:end
endlocal