@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0package-linux-wsl.ps1" %*
