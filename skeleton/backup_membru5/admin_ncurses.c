#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <time.h>

#define MAX_LINE 256

typedef struct admin_stats {
    char images_processed[64];
    char last_filename[256];
    char last_filesize[64];
    char last_client_id[64];
    char last_status[128];
    char last_refresh_time[64];
    char stats_file_status[64];
} admin_stats_t;

static void trim_newline(char* s) {
    size_t len;
    if (s == NULL) {
        return;
    }

    len = strlen(s);
    if (len > 0 && s[len - 1] == '\n') {
        s[len - 1] = '\0';
    }
}

static void set_default_stats(admin_stats_t* stats) {
    strcpy(stats->images_processed, "N/A");
    strcpy(stats->last_filename, "N/A");
    strcpy(stats->last_filesize, "N/A");
    strcpy(stats->last_client_id, "N/A");
    strcpy(stats->last_status, "N/A");
    strcpy(stats->last_refresh_time, "N/A");
    strcpy(stats->stats_file_status, "NOT FOUND");
}

static void parse_stat_line(const char* line, const char* key, char* dest, size_t dest_size) {
    size_t key_len = strlen(key);

    if (strncmp(line, key, key_len) == 0) {
        snprintf(dest, dest_size, "%s", line + key_len);
    }
}

static void set_current_time(char* buffer, size_t buffer_size) {
    time_t now;
    struct tm* tm_info;

    now = time(NULL);
    tm_info = localtime(&now);

    if (tm_info == NULL) {
        snprintf(buffer, buffer_size, "N/A");
        return;
    }

    strftime(buffer, buffer_size, "%H:%M:%S", tm_info);
}

static int load_stats(admin_stats_t* stats) {
    FILE* f;
    char line[MAX_LINE];

    if (stats == NULL) {
        return -1;
    }

    set_default_stats(stats);
    set_current_time(stats->last_refresh_time, sizeof(stats->last_refresh_time));

    f = fopen("stats.txt", "r");
    if (f == NULL) {
        return -1;
    }

    strcpy(stats->stats_file_status, "OK");

    while (fgets(line, sizeof(line), f) != NULL) {
        trim_newline(line);

        parse_stat_line(line, "images_processed=", stats->images_processed, sizeof(stats->images_processed));
        parse_stat_line(line, "last_filename=", stats->last_filename, sizeof(stats->last_filename));
        parse_stat_line(line, "last_filesize=", stats->last_filesize, sizeof(stats->last_filesize));
        parse_stat_line(line, "last_client_id=", stats->last_client_id, sizeof(stats->last_client_id));
        parse_stat_line(line, "last_status=", stats->last_status, sizeof(stats->last_status));
    }

    fclose(f);
    return 0;
}

static void draw_ui(const admin_stats_t* stats) {
    clear();

    mvprintw(1, 2, "======================================");
    mvprintw(2, 2, "         ADMIN MONITOR - NCURSES      ");
    mvprintw(3, 2, "======================================");

    mvprintw(5, 4, "Images processed : %s", stats->images_processed);
    mvprintw(6, 4, "Last filename    : %s", stats->last_filename);
    mvprintw(7, 4, "Last filesize    : %s", stats->last_filesize);
    mvprintw(8, 4, "Last client id   : %s", stats->last_client_id);
    mvprintw(9, 4, "Last status      : %s", stats->last_status);
    mvprintw(10, 4, "Last refresh     : %s", stats->last_refresh_time);
    mvprintw(11, 4, "Stats file       : %s", stats->stats_file_status);

    mvprintw(13, 2, "======================================");
    mvprintw(14, 2, "Auto refresh every 2 seconds");
    mvprintw(15, 2, "Press q to quit");

    refresh();
}

int main(void) {
    admin_stats_t stats;
    int ch;

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    timeout(2000);

    while (1) {
        load_stats(&stats);
        draw_ui(&stats);

        ch = getch();
        if (ch == 'q' || ch == 'Q') {
            break;
        }
    }

    endwin();
    return EXIT_SUCCESS;
}