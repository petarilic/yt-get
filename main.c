#define _CRT_SECURE_NO_WARNINGS
#define _WIN32_WINNT 0x0501

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#ifndef _WIN32
#define MAX_VIDEOS 500
#else
#define MAX_VIDEOS 500
#endif

#define TITLE_LEN 256
#define CMD_LEN 4096

typedef struct {
    char id[64];
    char title[TITLE_LEN];
} Video;

void sanitize_filename(char* name) {
    char* p = name;
    while (*p) {
        if (strchr("\\/:*?\"<>|", *p)) *p = '_';
        p++;
    }
}

int is_playlist(const char* url) {
    return strstr(url, "list=") != NULL;
}

int get_playlist_videos(const char* playlist_url, Video* videos, int* count) {
    char cmd[CMD_LEN];
    FILE* fp;
    *count = 0;

    if (!is_playlist(playlist_url)) {
        char cmd[CMD_LEN];
        snprintf(cmd, CMD_LEN, "yt-dlp.exe --print \"%(id)s\" \"%s\"", playlist_url);
        fp = _popen(cmd, "r");
        if (!fp) return -1;
        if (fgets(videos[0].id, 64, fp)) {
            char* ln = strchr(videos[0].id, '\n');
            if (ln) *ln = '\0';
        }
        _pclose(fp);

        snprintf(cmd, CMD_LEN, "yt-dlp.exe --print \"%(title)s\" \"%s\"", playlist_url);
        fp = _popen(cmd, "r");
        if (!fp) return -1;
        if (fgets(videos[0].title, TITLE_LEN, fp)) {
            char* ln = strchr(videos[0].title, '\n');
            if (ln) *ln = '\0';
        }
        _pclose(fp);

        *count = 1;
        return 1;
    }

    snprintf(cmd, CMD_LEN,
        "yt-dlp.exe -q --flat-playlist --get-id --get-title --playlist-start 1 --playlist-end %d \"%s\" > downloads\\playlist.txt",
        MAX_VIDEOS, playlist_url);

    system(cmd);

    fp = fopen("downloads\\playlist.txt", "r");
    if (!fp) return -1;

    char line[2048];
    int line_idx = 0;
    while (fgets(line, sizeof(line), fp)) {
        char* ln = strchr(line, '\n');
        if (ln) *ln = '\0';
        if (line_idx % 2 == 0) {
            if (*count < MAX_VIDEOS && line[0]) {
                strncpy(videos[*count].title, line, TITLE_LEN - 1);
                videos[*count].title[TITLE_LEN - 1] = '\0';
            }
        } else {
            if (*count < MAX_VIDEOS) {
                strncpy(videos[*count].id, line, 63);
                videos[*count].id[63] = '\0';
                (*count)++;
            }
        }
        line_idx++;
    }
    fclose(fp);
    return *count;
}

//
int download_video(Video* video, const char* url, int format_video, const char* ffmpeg_path) {
    char cmd[CMD_LEN];
    sanitize_filename(video->title);
    printf("Downloading: %s\n", video->title);
    if (format_video) {
        snprintf(cmd, CMD_LEN,
            "yt-dlp.exe -q -f bestvideo+bestaudio -o \"downloads\\\\%s.%%(ext)s\" \"%s\"",
            video->title, url);
    } else {
        snprintf(cmd, CMD_LEN,
            "yt-dlp.exe -q --ffmpeg \"%s\" --extract-audio --audio-format mp3 -o \"downloads\\\\%s.%%(ext)s\" \"%s\"",
            ffmpeg_path, video->title, url);
    }
    return system(cmd);
}

int create_zip_archive(const char* zipname, const char* ext) {
    char cmd[CMD_LEN];
    snprintf(cmd, CMD_LEN, "powershell -Command \"Compress-Archive -Path 'downloads\\*.%s' -DestinationPath '%s' -Force\"", ext, zipname);
    return system(cmd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s -u <URL> -f <mp3|mp4> [output.zip]\n", argv[0]);
        printf("Example: %s -u \"https://youtube.com/...\" -f mp3\n", argv[0]);
        return 1;
    }

    const char* playlist_url = NULL;
    const char* zipname = "playlist.zip";
    int format_video = 0;
    const char* ext = "mp3";
    const char* ffmpeg_path = ".\\ffmpeg.exe";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
            playlist_url = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "mp4") == 0 || strcmp(argv[i], "video") == 0) {
                format_video = 1;
                ext = "mp4";
            } else if (strcmp(argv[i], "mp3") == 0) {
                format_video = 0;
                ext = "mp3";
            }
        } else if (argv[i][0] != '-') {
            zipname = argv[i];
        }
    }

    if (!playlist_url) {
        printf("Error: missing -u <URL>\n");
        return 1;
    }

    system("mkdir downloads 2>nul");
    system("del downloads\\playlist.txt 2>nul");

    printf("Processing: %s\n", playlist_url);
    printf("Format: %s\n\n", format_video ? "mp4" : "mp3");

    Video videos[MAX_VIDEOS];
    int count = 0;

    if (get_playlist_videos(playlist_url, videos, &count) <= 0) {
        printf("Error fetching playlist\n");
        return 1;
    }

    printf("Found %d videos\n\n", count);

    for (int i = 0; i < count; i++) {
        printf("[%d/%d] ", i + 1, count);
        char url[512];
        if (is_playlist(playlist_url)) {
            snprintf(url, sizeof(url), "https://www.youtube.com/watch?v=%s", videos[i].id);
        } else {
            snprintf(url, sizeof(url), "%s", playlist_url);
        }
        download_video(&videos[i], url, format_video, ffmpeg_path);
    }

    printf("\nCreating ZIP: %s\n", zipname);
    int ret = create_zip_archive(zipname, ext);
    if (ret == 0) {
        printf("Done! Created: %s\n", zipname);
    }

    return ret;
    
}