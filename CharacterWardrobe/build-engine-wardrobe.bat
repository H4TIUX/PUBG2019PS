@echo off
setlocal EnableExtensions
cd /d "%~dp0..\build-source\PUBG2019PS"
if not exist "..\..\build" mkdir "..\..\build"
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "HOST_FLAGS=/nologo /std:c++latest /EHsc /O2 /MT /w /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /FIcstdint /FIcmath /FIstring /FIalgorithm /FIconcepts /FIWindows.h /I. /I..\minhook_src"
cl %HOST_FLAGS% /c "..\..\CharacterWardrobe\EngineWardrobeProbe.cpp" /Fo"..\..\CharacterWardrobe\EngineWardrobeProbe.obj"
if errorlevel 1 exit /b 1
link /nologo /DLL /MACHINE:X64 /OUT:"..\..\CharacterWardrobe\EngineWardrobeProbe.dll" "..\..\CharacterWardrobe\EngineWardrobeProbe.obj" "..\..\build\unit_01.obj" "..\..\build\unit_04.obj" "..\..\build\unit_07.obj" "..\..\build\unit_08.obj" "..\..\build\unit_09.obj" "..\..\build\unit_10.obj" "..\..\build\unit_11.obj" "..\..\build\unit_12.obj" "..\..\build\minhook_buffer.obj" "..\..\build\minhook_hook.obj" "..\..\build\minhook_trampoline.obj" user32.lib ws2_32.lib
exit /b %errorlevel%
