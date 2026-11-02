@echo off
cd /d "%~dp0"
echo Installing/checking PyInstaller...
python -m pip install pyinstaller
echo Building a windowed executable...
pyinstaller --noconsole --onefile --name RemoteMapperHost remotemapper_host.py
echo.
echo [OK] Build complete: dist\RemoteMapperHost.exe
pause
