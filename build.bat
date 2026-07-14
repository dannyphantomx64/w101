@echo off
setlocal

echo ================================================
echo  W101 Intelligence Suite - Build Script
echo ================================================
echo.

:: Find VS2022
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] Visual Studio not found. Install VS2022 with C++ workload.
    exit /b 1
)

for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VS_PATH=%%i"
echo [INFO] Visual Studio: %VS_PATH%

:: Find CMake (bundled with VS)
set "CMAKE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" (
    echo [WARN] VS CMake not found, trying system cmake...
    where cmake >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] CMake not found. Install CMake or VS2022 with C++ CMake tools.
        exit /b 1
    )
    set "CMAKE=cmake"
)
echo [INFO] CMake: %CMAKE%

:: Create build directory
if not exist build mkdir build
cd build

:: Configure
echo.
echo [BUILD] Configuring x64...
"%CMAKE%" -G "Visual Studio 17 2022" -A x64 ..
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

:: Build Release
echo.
echo [BUILD] Compiling Release...
"%CMAKE%" --build . --config Release --parallel
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

:: Copy output
echo.
set "DLL=Release\w101suite.dll"
set "EXE=Release\w101inject.exe"
if exist "%DLL%" (
    copy /Y "%DLL%" "..\w101suite.dll" >nul
    echo [OK] Built: w101suite.dll
    for %%A in ("%DLL%") do echo        %%~zA bytes
) else (
    echo [WARN] DLL not found at expected path
)
if exist "%EXE%" (
    copy /Y "%EXE%" "..\w101inject.exe" >nul
    echo [OK] Built: w101inject.exe
    for %%A in ("%EXE%") do echo        %%~zA bytes
)

cd ..
echo.
echo ================================================
echo  Build complete. Inject w101suite.dll into
echo  WizardGraphicalClient.exe
echo ================================================
