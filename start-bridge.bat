@echo off
cd /d "%~dp0"

rem Starts only the timer bridge, with a visible minimized console for
rem debugging. Launch the game through Steam as usual.

start "" /min python "%~dp0speedrun-bridge.py" --source surface --surface-file "%~dp0speedrun-surface.bin" --events "%~dp0speedrun-events.log" --idle-timeout 300
