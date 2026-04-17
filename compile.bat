@echo off
echo Compiling yt-get playlist downloader...
gcc -o yt-get.exe main.c -std=c99
if errorlevel 1 (
    echo Compilation failed!
    exit /b 1
)
echo Done! Run: yt-get.exe "https://www.youtube.com/playlist?list=..." [output.zip]