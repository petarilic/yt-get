@echo off
echo Compiling yt-get playlist downloader...
gcc -o yt-get.exe main.c -std=c99 -static
if errorlevel 1 (
    echo Compilation failed!
    
    exit /b 1
)
echo.
echo Compilation successful!
echo.
echo Usage: yt-get.exe -u "URL" -f [mp3^|mp4] [output.zip]
echo Example: yt-get.exe -u "https://youtube.com/watch?v=..." -f mp3 myfiles.zip
echo.
echo Note: yt-dlp.exe must be in the same directory as yt-get.exe