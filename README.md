# yt-get

CLI program to download YouTube playlists as MP3 or MP4

## Usage

```bash
yt-get.exe -u "URL" -f [mp3|mp4] -z [name.zip]
```

Arguments:
- `-u` - YouTube playlist or video URL (required)
- `-f` - Output format: `mp3` (audio) or `mp4` (video) (optional, default: mp3)
- `-z` - ZIP file name (optional, default: playlist.zip)

## Example

```bash
yt-get.exe -in "https://www.youtube.com/playlist?list=PLDash8fH1DySRIECg9CLe2d8_8"
yt-get.exe -u "https://www.youtube.com/playlist?list=PL..." -f mp3 -z mojapevnjica.zip
in yt-get.exe "https://www.youtube.com/watch?v=..." -f mp4 -z myvideo.zip
```

## Requests

- [yt-dlp](https://github.com/yt-dlp/yt-dlp) - to download video and convert to MP3
- [Deno](https://deno.land/) - JavaScript runtime for yt-dlp (optional, recommended for better results)
- ffmpeg - in program directory or in PATH (for mp3 conversion)
- PowerShell - to create a ZIP archive
- GCC - for compiling (optional)

## Compiling

```bash
compile.bat
```

Or manually:

```bash
gcc -o yt-get.exe main.c -std=c99
```

## How it works

1. Parse the playlist using `yt-dlp --flat-playlist`
2. For each video call `yt-dlp -x --audio-format mp3` (mp3) or `yt-dlp -f best[ext=mp4]/best` (mp4)
3. Packs all files into a ZIP archive using PowerShell

## Note

- The program runs on Windows
- You need to have yt-dlp in PATH or in the same directory
- mp3 conversion requires ffmpeg in directory or PATH
- If JavaScript runtime warnings appear, install [Deno](https://deno.land/) for better results