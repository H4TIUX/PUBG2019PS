@echo off
setlocal EnableExtensions
cd /d "%~dp0..\..\build-source\PUBG2019PS"
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "HOST_FLAGS=/nologo /std:c++latest /EHsc /O2 /MT /w /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /DNDEBUG %SKIN_TEST_FLAGS% /FIcstdint /FIcmath /FIstring /FIalgorithm /FIconcepts /FIWindows.h /I. /I..\..\testing\port-2017-improvements /I..\minhook_src"
cl %HOST_FLAGS% /c "SDK\CustomizableObject_functions.cpp" /Fo"..\..\launcher-current\server\CustomizableObject.obj"
if errorlevel 1 exit /b 1
cl %HOST_FLAGS% /c "SDK\PhysXVehicles_functions.cpp" /Fo"..\..\launcher-current\server\PhysXVehicles_functions-drivefix.obj"
if errorlevel 1 exit /b 1
cl %HOST_FLAGS% /c "..\..\launcher-current\server\BotLogic.cpp" /Fo"..\..\launcher-current\server\BotLogic.obj"
if errorlevel 1 exit /b 1
cl %HOST_FLAGS% /c "..\..\launcher-current\server\dllmain.cpp" /Fo"..\..\launcher-current\server\dllmain.obj"
if errorlevel 1 exit /b 1
link /nologo /DLL /MAP:"..\..\launcher-current\server\PUBG2019PS-squad.map" /MACHINE:X64 /OUT:"..\..\launcher-current\server\PUBG2019PS-squad.dll" "..\..\launcher-current\server\CustomizableObject.obj" "..\..\build\unit_00.obj" "..\..\build\unit_01.obj" "..\..\build\unit_02.obj" "..\..\launcher-current\server\BotLogic.obj" "..\..\build\unit_04.obj" "..\..\launcher-current\server\dllmain.obj" "..\..\build\unit_06.obj" "..\..\build\unit_07.obj" "..\..\build\unit_08.obj" "..\..\build\unit_09.obj" "..\..\build\unit_10.obj" "..\..\build\unit_11.obj" "..\..\build\unit_12.obj" "..\..\launcher-current\server\PhysXVehicles_functions-drivefix.obj" "..\..\build\minhook_buffer.obj" "..\..\build\minhook_hook.obj" "..\..\build\minhook_trampoline.obj" user32.lib ws2_32.lib
exit /b %errorlevel%
