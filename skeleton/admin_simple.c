#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 256

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

static void print_field(const char* line, const char* key, const char* label) {
    size_t key_len = strlen(key);

    if (strncmp(line, key, key_len) == 0) {
        printf("%-18s: %s\n", label, line + key_len);
    }
}

static void display_stats(void) {
    FILE* f;
    char line[MAX_LINE];

    f = fopen("stats.txt", "r");
    if (f == NULL) {
        printf("Could not open stats.txt\n");
        return;
    }

    printf("=====================================\n");
    printf("           ADMIN MONITOR             \n");
    printf("=====================================\n");

    while (fgets(line, sizeof(line), f) != NULL) {
        trim_newline(line);

        print_field(line, "images_processed=", "Images processed");
        print_field(line, "last_filename=", "Last filename");
        print_field(line, "last_filesize=", "Last filesize");
        print_field(line, "last_client_id=", "Last client id");
        print_field(line, "last_status=", "Last status");
    }

    printf("=====================================\n");
    printf("Refreshing every 2 seconds...\n");

    fclose(f);
}

int main(void) {
    while (1) {
        system("clear");
        display_stats();
        sleep(2);
    }

    return EXIT_SUCCESS;
}