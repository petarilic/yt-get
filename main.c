/**
 * yt-get: YouTube Playlist Downloader
 * Features: Progress bar, per-session folders, proper cleanup
 */

#define _CRT_SECURE_NO_WARNINGS
#define _WIN32_WINNT 0x0501

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h>  // _popen/_pclose
#include <direct.h>   // _mkdir
#include <errno.h>    // errno, EEXIST
#include <time.h>     // time()
#include <process.h>  // _popen, _pclose

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define MAX_VIDEOS 500
#define TITLE_LEN 256
#define CMD_LEN 4096

typedef struct {
    char id[64];
    char title[TITLE_LEN];
} Video;

// Sanitize filename by replacing invalid characters
void sanitize_filename(char* name) {
    char* p = name;
    while (*p) {
        if (strchr("\\/:*?\"<>|", *p)) *p = '_';
        p++;
    }
}

// Check if URL is a playlist
int is_playlist(const char* url) {
    return strstr(url, "list=") != NULL;
}

// Progress thread data
typedef struct {
    volatile int percent;
    volatile char spinner_char;
    HANDLE stop_event;
    int index;
    int total;
    char title[TITLE_LEN];
} ProgressData;

// Progress thread - updates display with spinner and percentage
DWORD WINAPI ProgressThread(LPVOID lpParam) {
    ProgressData* pd = (ProgressData*)lpParam;
    const char spinner_chars[] = "-\\|/";
    int idx = 0;
    int last_percent = -1;
    char last_spinner = 0;
    int bar_width = 20;
    
    while (WaitForSingleObject(pd->stop_event, 0) != WAIT_OBJECT_0) {
        pd->spinner_char = spinner_chars[idx];
        idx = (idx + 1) % 4;
        
        if (pd->percent != last_percent || pd->spinner_char != last_spinner) {
    int pos = pd->percent * bar_width / 100;
            // Compact: [1/1] Title [spinner] 23% [===...]
            printf("\r[%d/%d] %-20.20s [%c] %3d%% [",
                   pd->index, pd->total, pd->title, pd->spinner_char, pd->percent);
            for (int i = 0; i < bar_width; i++) {
                putchar(i < pos ? '=' : ' ');
            }
            printf("]");
            fflush(stdout);
            last_percent = pd->percent;
            last_spinner = pd->spinner_char;
        }
        Sleep(100);
    }
    return 0;
}

// Get videos from playlist or single video
int get_playlist_videos(const char* playlist_url, Video* videos, int* count, char* session_dir) {
    char cmd[CMD_LEN];
    FILE* fp;
    *count = 0;

    // Single video - get id and title from URL
    if (!is_playlist(playlist_url)) {
        char id_cmd[CMD_LEN];
        char title_cmd[CMD_LEN];
        int got_id = 0, got_title = 0;
        
        // Get video ID
        snprintf(id_cmd, CMD_LEN, "yt-dlp.exe --print \"%%(id)s\" --no-warnings \"%s\"", playlist_url);
        fp = _popen(id_cmd, "r");
        if (!fp) {
            printf("Error: Failed to run yt-dlp for ID extraction\n");
            return -1;
        }
        if (fgets(videos[0].id, sizeof(videos[0].id), fp)) {
            char* ln = strchr(videos[0].id, '\n');
            if (ln) *ln = '\0';
            if (videos[0].id[0]) got_id = 1;
        }
        _pclose(fp);

        if (!got_id) {
            printf("Error: Could not extract video ID. Check URL and network connection.\n");
            return -1;
        }

        // Get video title
        snprintf(title_cmd, CMD_LEN, "yt-dlp.exe --print \"%%(title)s\" --no-warnings \"%s\"", playlist_url);
        fp = _popen(title_cmd, "r");
        if (!fp) {
            printf("Error: Failed to run yt-dlp for title extraction\n");
            return -1;
        }
        if (fgets(videos[0].title, sizeof(videos[0].title), fp)) {
            char* ln = strchr(videos[0].title, '\n');
            if (ln) *ln = '\0';
            if (videos[0].title[0]) got_title = 1;
        }
        _pclose(fp);

        if (!got_title) {
            printf("Error: Could not extract video title. Check URL and network connection.\n");
            return -1;
        }

        sanitize_filename(videos[0].title);
        *count = 1;
        return 1;
    }

    // Playlist - use temporary file
    char temp_file[MAX_PATH];
    snprintf(temp_file, MAX_PATH, "%s\\playlist.txt", session_dir);
    
    snprintf(cmd, CMD_LEN,
        "yt-dlp.exe -q --flat-playlist --get-id --get-title \"%s\" > \"%s\"",
        playlist_url, temp_file);
    int sys_ret = system(cmd);
    if (sys_ret != 0) {
        printf("Error: yt-dlp failed to fetch playlist (code: %d)\n", sys_ret);
        return -1;
    }

    fp = fopen(temp_file, "r");
    if (!fp) {
        printf("Error: Could not open playlist file. Path: %s\n", temp_file);
        return -1;
    }

    char line[2048];
    int line_idx = 0;
    while (fgets(line, sizeof(line), fp) && *count < MAX_VIDEOS) {
        char* ln = strchr(line, '\n');
        if (ln) *ln = '\0';
        
        if (line_idx % 2 == 0) {
            strncpy(videos[*count].title, line, TITLE_LEN - 1);
            videos[*count].title[TITLE_LEN - 1] = '\0';
            sanitize_filename(videos[*count].title);
        } else {
            strncpy(videos[*count].id, line, sizeof(videos[*count].id) - 1);
            videos[*count].id[sizeof(videos[*count].id) - 1] = '\0';
            (*count)++;
        }
        line_idx++;
    }
    fclose(fp);
    return *count;
}

// Download single video with real-time progress tracking
int download_video(Video* video, const char* url, int format_video, 
                   const char* ffmpeg_path, const char* session_dir, int index, int total) {
    char cmd[CMD_LEN];
    char output_path[MAX_PATH];
    ProgressData pd;
    
    // Initialize progress data
    pd.percent = 0;
    pd.index = index + 1;
    pd.total = total;
    strncpy(pd.title, video->title, TITLE_LEN - 1);
    pd.title[TITLE_LEN - 1] = '\0';
    pd.stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    pd.spinner_char = '-';
    
    // Start progress thread
    HANDLE progress_thread = CreateThread(NULL, 0, ProgressThread, &pd, 0, NULL);
    
    // Build output path
    snprintf(output_path, MAX_PATH, "%s\\%s.%%(ext)s", session_dir, video->title);
    
    // Build command with progress output piped
    if (format_video) {
        snprintf(cmd, CMD_LEN,
            "yt-dlp.exe -q --newline --progress -f bestvideo+bestaudio -o \"%s\" \"%s\" 2>&1",
            output_path, url);
    } else {
        snprintf(cmd, CMD_LEN,
            "yt-dlp.exe -q --newline --progress --ffmpeg \"%s\" --extract-audio --audio-format mp3 -o \"%s\" \"%s\" 2>&1",
            ffmpeg_path, output_path, url);
    }

    // Open pipe to read yt-dlp output
    FILE* pipe = _popen(cmd, "r");
    if (!pipe) {
        printf("Error: Could not start download process\n");
        return -1;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        // Look for progress percentage in lines containing [download]
        if (strstr(buffer, "[download]")) {
            float fpercent = 0.0;
            // Skip to first digit and read float followed by %
            if (sscanf(buffer, "%*[^0-9]%f%%", &fpercent) == 1) {
                pd.percent = (int)fpercent;
            }
        }
    }
    
    int ret = _pclose(pipe);
    pd.percent = ret == 0 ? 100 : pd.percent;  // set to 100% on success
    
    // Stop progress thread
    SetEvent(pd.stop_event);
    WaitForSingleObject(progress_thread, INFINITE);
    CloseHandle(progress_thread);
    CloseHandle(pd.stop_event);
    
    // Final status - clear bar area with padding
    printf("\r[%d/%d] %-20.20s %-5s%*s\n", index + 1, total, video->title,
           ret == 0 ? "Done" : "Failed", 35, "");
    return ret;
}

// Create zip archive from session directory
int create_zip_archive(const char* zipname, const char* session_dir, const char* ext) {
    char cmd[CMD_LEN];
    char pattern[MAX_PATH];
    
    snprintf(pattern, MAX_PATH, "%s\\*.%s", session_dir, ext);
    snprintf(cmd, CMD_LEN,
        "powershell -Command \"Compress-Archive -Path '%s' -DestinationPath '%s' -Force\"",
        pattern, zipname);
    
    return system(cmd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s -u <URL> -f <mp3|mp4> [output.zip]\n", argv[0]);
        printf("Example: %s -u \"https://youtube.com/...\" -f mp3\n", argv[0]);
        return 1;
    }

    const char* playlist_url = NULL;
    char zipname[MAX_PATH] = "playlist.zip";
    int format_video = 0;
    const char* ext = "mp3";
    const char* ffmpeg_path = ".\\ffmpeg.exe";
    char session_dir[MAX_PATH];

    // Parse arguments
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
            strncpy(zipname, argv[i], MAX_PATH - 1);
            zipname[MAX_PATH - 1] = '\0';
        }
    }

    if (!playlist_url) {
        printf("Error: missing -u <URL>\n");
        return 1;
    }

    // Ensure base downloads directory exists
    _mkdir("downloads");
    if (_mkdir("downloads") != 0 && errno != EEXIST) {
        printf("Error: Could not create downloads directory\n");
        return 1;
    }

    // Create session directory based on playlist/video title
    char session_name[MAX_PATH] = "session";
    
    // Fetch title for naming BEFORE downloading
    if (is_playlist(playlist_url)) {
        // Extract playlist ID from URL for folder name
        char* start = strstr(playlist_url, "list=");
        if (start) {
            char list_id[64] = {0};
            start += 5;
            char* end = strchr(start, '&');
            int len = end ? (int)(end - start) : strlen(start);
            if (len > 63) len = 63;
            strncpy(list_id, start, len);
            list_id[len] = '\0';
            snprintf(session_name, MAX_PATH, "playlist_%s", list_id);
        } else {
            snprintf(session_name, MAX_PATH, "playlist_unknown");
        }
    } else {
        // Single video - try to get video ID for folder name
        char video_id[64] = {0};
        char temp_cmd[CMD_LEN];
        FILE* temp_fp;
        
        snprintf(temp_cmd, CMD_LEN, "yt-dlp.exe --print \"%%(id)s\" --no-warnings \"%s\"", playlist_url);
        temp_fp = _popen(temp_cmd, "r");
        if (temp_fp && fgets(video_id, sizeof(video_id), temp_fp)) {
            char* ln = strchr(video_id, '\n');
            if (ln) *ln = '\0';
        }
        if (temp_fp) _pclose(temp_fp);
        
        if (video_id[0]) {
            snprintf(session_name, MAX_PATH, "video_%s", video_id);
        } else {
            time_t now = time(NULL);
            snprintf(session_name, MAX_PATH, "video_%ld", now);
        }
    }
    
    snprintf(session_dir, MAX_PATH, "downloads\\%s", session_name);
    if (_mkdir(session_dir) != 0 && errno != EEXIST) {
        printf("Error: Could not create session directory: %s\n", session_dir);
        return 1;
    }

    printf("Processing: %s\n", playlist_url);
    printf("Format: %s\n", format_video ? "mp4" : "mp3");
    printf("Output: %s\n\n", session_dir);

    Video videos[MAX_VIDEOS];
    int count = 0;

    if (get_playlist_videos(playlist_url, videos, &count, session_dir) <= 0) {
        printf("Error: Could not fetch playlist/video information\n");
        return 1;
    }

    printf("Found %d video(s)\n\n", count);

    // Download each video with progress
    for (int i = 0; i < count; i++) {
        char url[512];
        if (is_playlist(playlist_url)) {
            snprintf(url, sizeof(url), "https://www.youtube.com/watch?v=%s", videos[i].id);
        } else {
            snprintf(url, sizeof(url), "%s", playlist_url);
        }
        
        download_video(&videos[i], url, format_video, ffmpeg_path, 
                      session_dir, i, count);
    }

    printf("\nCreating ZIP: %s\n", zipname);
    int ret = create_zip_archive(zipname, session_dir, ext);
    
    if (ret == 0) {
        printf("Done! Created: %s\n", zipname);
        printf("Session files saved in: %s\n", session_dir);
    } else {
        printf("Failed to create ZIP archive\n");
    }

    return ret;
}
