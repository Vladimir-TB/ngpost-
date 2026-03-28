@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-linux-wsl.ps1" %*
