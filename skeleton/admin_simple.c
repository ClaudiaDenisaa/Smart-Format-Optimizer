/**
 * IR3 2026
 * monitor admin simplu
 *
 * Programul afiseaza in terminal statisticile citite din fisierul stats.txt.
 * Informatiile sunt reincarcate automat la fiecare 2 secunde.
 *
 * Sunt afisate:
 *  - numarul de imagini procesate
 *  - ultimul fisier procesat
 *  - ultima dimensiune de fisier
 *  - ultimul client
 *  - ultimul status
 */

#include <stdio.h>   /* utilizat pentru: FILE, fopen, fclose, fgets, printf */
#include <stdlib.h>  /* utilizat pentru: EXIT_SUCCESS, system */
#include <string.h>  /* utilizat pentru: strlen, strncmp */
#include <unistd.h>  /* utilizat pentru: sleep */

#define MAX_LINE 256

/*
 * Elimina caracterul '\n' de la finalul sirului, daca exista.
 * Este util dupa citirea unei linii din fisier cu fgets().
 */
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

/*
 * Verifica daca linia incepe cu cheia primita.
 * Daca da, afiseaza valoarea gasita intr-un format mai clar.
 *
 * Exemplu:
 * line = "images_processed=12"
 * key = "images_processed="
 * label = "Images processed"
 */
static void print_field(const char* line, const char* key, const char* label) {
    size_t key_len = strlen(key);

    if (strncmp(line, key, key_len) == 0) {
        printf("%-18s: %s\n", label, line + key_len);
    }
}

/*
 * Deschide fisierul stats.txt si afiseaza statisticile gasite in el.
 * Daca fisierul nu poate fi deschis, se afiseaza un mesaj de eroare.
 */
static void display_stats(void) {
    FILE* f;
    char line[MAX_LINE];

    /* incercam sa deschidem fisierul cu statistici */
    f = fopen("stats.txt", "r");
    if (f == NULL) {
        printf("Could not open stats.txt\n");
        return;
    }

    /* afisam antetul monitorului */
    printf("=====================================\n");
    printf("           ADMIN MONITOR             \n");
    printf("=====================================\n");

    /* citim fisierul linie cu linie */
    while (fgets(line, sizeof(line), f) != NULL) {
        trim_newline(line);

        /* verificam fiecare camp cunoscut si il afisam */
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

/*
 * Functia principala ruleaza monitorul intr-o bucla infinita.
 * La fiecare iteratie:
 *  - sterge ecranul
 *  - afiseaza statisticile
 *  - asteapta 2 secunde
 */
int main(void) {
    while (1) {
        system("clear");
        display_stats();
        sleep(2);
    }

    return EXIT_SUCCESS;
}

/*
Exemple de rulare:
make
./admin_simple

Comportament:
- programul citeste datele din fisierul stats.txt
- afiseaza statisticile in terminal
- actualizeaza automat informatiile la fiecare 2 secunde
- ruleaza continuu pana este oprit manual
*/