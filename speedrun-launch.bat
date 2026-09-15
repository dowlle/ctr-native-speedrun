@echo off
setlocal
cd /d "%~dp0"

rem Starts the LiveSplit bridge, then the client. The bridge exits on its own
rem when the client stops updating the surface.

where python >nul 2>&1
if %ERRORLEVEL%==0 (
    start "" /min python "%~dp0speedrun-bridge.py" --source surface --surface-file "%~dp0speedrun-surface.bin" --events "%~dp0speedrun-events.log" --idle-timeout 20
) else (
    echo [speedrun] python not found; starting the client without the timer bridge.
)

"%~dp0ctr_native.exe"
endlocal
