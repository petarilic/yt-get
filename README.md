# yt-get

CLI program za preuzimanje YouTube plejlista kao MP3 ili MP4 

## Koriscenje

```bash
yt-get.exe -u "URL" -f [mp3|mp4] [ime.zip]
```

Argumenti:
- `-u` - URL YouTube plejliste ili videa (obavezan)
- `-f` - Format izlaza: `mp3` (audio) ili `mp4` (video) (opciono, podrazumevano: mp3)
- `output_zip` - Ime ZIP fajla (opciono, podrazumevano: playlist.zip)

## Primer

```bash
yt-get.exe -u "https://www.youtube.com/playlist?list=PLDash8fH1DySRIECg9CLe2d8_8"
yt-get.exe -u "https://www.youtube.com/playlist?list=PL..." -f mp3 mojapevnjica.zip
yt-get.exe -u "https://www.youtube.com/watch?v=..." -f mp4 mojvideo.zip
```

## Zahtevi

- [yt-dlp](https://github.com/yt-dlp/yt-dlp) - za preuzimanje videa i konverziju u MP3
- ffmpeg - u direktorijumu programa ili u PATH-u (za mp3 konverziju)
- PowerShell - za kreiranje ZIP arhive
- GCC - za kompajliranje (opciono)

## Kompajliranje

```bash
compile.bat
```

Ili rucno:

```bash
gcc -o yt-get.exe main.c -std=c99
```

## Kako radi

1. Parsira plejlistu pomocu `yt-dlp --flat-playlist`
2. Za svaki video poziva `yt-dlp --extract-audio --audio-format mp3` (mp3) ili `yt-dlp -f bestvideo+bestaudio` (mp4)
3. Pakuje sve fajlove u ZIP arhivu pomocu PowerShell

## Napomena

- Program radi na Windows-u
- Potrebno je imati yt-dlp u PATH-u ili u istom direktorijumu
- Za mp3 konverziju potreban je ffmpeg u direktorijumu ili PATH-u