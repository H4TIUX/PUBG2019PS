@echo off
cd /d "%~dp0"
"%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:exe /optimize+ /r:System.Web.Extensions.dll /out:PUBG2019NetDirectory.exe DirectoryServer.cs
exit /b %errorlevel%
