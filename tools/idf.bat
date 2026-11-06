@echo off
set "MSYSTEM="
set "MINGW_PREFIX="
set "IDF_TOOLS_PATH=D:\esp\.tools"
call "D:\esp\v5.5\esp-idf\export.bat"
cd /d "%~dp0.."
idf.py %*
