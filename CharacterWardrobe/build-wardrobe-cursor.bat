@echo off
setlocal EnableExtensions
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /TC /c "..\build-source\minhook_src\hde\hde64.c" /FoWardrobeHde64.obj
if errorlevel 1 exit /b 1
cl /nologo /EHsc /O2 /MT /LD /I"..\build-source\minhook_src" WardrobeCursor.cpp WardrobeHde64.obj "..\build\minhook_buffer.obj" "..\build\minhook_hook.obj" "..\build\minhook_trampoline.obj" /link /OUT:WardrobeCursor.dll user32.lib
exit /b %errorlevel%
