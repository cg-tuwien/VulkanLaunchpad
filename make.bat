@ECHO OFF
if "%~1"=="" goto BLANK
if "%~1"=="install" goto install
if "%~1"=="clean" goto CLEAN
@ECHO ON

:BLANK
cmake -H. -B_project -A "x64" -DCMAKE_INSTALL_PREFIX="_install"
GOTO DONE

:INSTALL
set @location="_install"
if NOT "%2"=="" set @location="%2"
cmake -H. -B_project -A "x64" -DCMAKE_INSTALL_PREFIX=%@location% 
cmake --build _project --config Release --target INSTALL
GOTO DONE

:CLEAN
rmdir /Q /S _project
rmdir /Q /S _install
GOTO DONE

:DONE