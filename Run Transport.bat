@echo off
cd /d "%~dp0"
where g++ >nul 2>nul
if errorlevel 1 (
    echo Install g++ and add it to PATH before building.
    pause
    exit /b 1
)
echo Building the transport simulator...
g++ -std=c++11 -Wall -Wextra -Wpedantic -O2 Network.cpp Simulation.cpp Main.cpp -o TransportSystem.exe
if errorlevel 1 (
    echo Build failed. Please check the errors above.
    pause
    exit /b 1
)
TransportSystem.exe
pause
