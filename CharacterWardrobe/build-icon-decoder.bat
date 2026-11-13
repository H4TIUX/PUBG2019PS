@echo off
setlocal
cd /d "%~dp0"
"%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:library /optimize+ /out:WardrobeIconDecoder.dll /reference:System.Drawing.dll WardrobeIconDecoder.cs
exit /b %errorlevel%
