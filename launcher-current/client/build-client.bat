@echo off
setlocal EnableExtensions
cd /d "D:\ftp\apps\vibecode\pubg2019\build-source\PUBG2019PS"
if not exist "..\..\build" mkdir "..\..\build"
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "HOST_FLAGS=/nologo /std:c++latest /EHsc /O2 /MT /w /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /FIcstdint /FIcmath /FIstring /FIalgorithm /FIconcepts /FIWindows.h /I. /I..\minhook_src"
cl %HOST_FLAGS% /Gy /c "SDK\CustomizableObject_functions.cpp" /Fo"..\..\launcher-current\client\CustomizableObject.obj"
if errorlevel 1 exit /b 1
cl %HOST_FLAGS% /Gy /c "SDK\UMG_functions.cpp" /Fo"..\..\launcher-current\client\UMG_functions.obj"
if errorlevel 1 exit /b 1
cl %HOST_FLAGS% /c "..\..\launcher-current\client\ClientPlayerName.cpp" /Fo"..\..\launcher-current\client\ClientPlayerName.obj"
if errorlevel 1 exit /b 1
link /nologo /DLL /OPT:REF /MACHINE:X64 /OUT:"..\..\launcher-current\client\PUBG2019PlayerName.dll" "..\..\launcher-current\client\ClientPlayerName.obj" "..\..\launcher-current\client\UMG_functions.obj" "..\..\launcher-current\client\CustomizableObject.obj" "..\..\build\unit_01.obj" "..\..\build\unit_04.obj" "..\..\build\unit_07.obj" "..\..\build\unit_08.obj" "..\..\build\unit_09.obj" "..\..\build\unit_10.obj" "..\..\build\unit_11.obj" "..\..\build\unit_12.obj" "..\..\build\minhook_buffer.obj" "..\..\build\minhook_hook.obj" "..\..\build\minhook_trampoline.obj" user32.lib ws2_32.lib winmm.lib
exit /b %errorlevel%
