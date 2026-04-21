/**
 * IR3 2026
 * Programul realizeaza pornirea firelor principale ale serverului.
 *
 * Sunt create fire separate pentru:
 * - comunicarea prin socket UNIX
 * - comunicarea prin socket Internet
 * - partea SOAP
 * - firul worker care preia imaginile din coada si le proceseaza
 *
 * Programul citeste si configuratia serverului din fisierul server.cfg.
 */

#include <stddef.h>      /* utilizat pentru: size_t */
#include <errno.h>       /* utilizat pentru coduri de eroare */
#include <pthread.h>     /* utilizat pentru threaduri si mutex */
#include <stdio.h>       /* utilizat pentru: fprintf */
#include <stdlib.h>      /* utilizat pentru: exit, NULL */
#include <string.h>      /* utilizat pentru functii pe siruri */
#include <unistd.h>      /* utilizat pentru: unlink */
#include <sys/types.h>   /* tipuri de baza pentru socket */
#include <sys/socket.h>  /* utilizat pentru operatii pe socket */
#include <sys/select.h>  /* utilizat pentru select */
#include <libconfig.h>   /* utilizat pentru citirea fisierului de configurare */

#include "queue.h"            /* coada de taskuri cu imagini */
#include "image_processor.h"  /* functia de procesare a imaginilor */

/* prototipuri pentru firele principale ale serverului */
void *unix_main (void *args);
void *inet_main (void *args);
void *soap_main (void *args);
void *worker_main (void *args);

/* calea socketului UNIX */
#define UNIXSOCKET "/tmp/unixds"

/* portul folosit pentru componenta SOAP */
#define SOAPPORT   18082

/* mutex global folosit in alte zone ale aplicatiei */
pthread_mutex_t curmtx = PTHREAD_MUTEX_INITIALIZER;

int main () {
    config_t cfg;
    int iport, sport;
    pthread_t unixthr, inetthr, soapthr, workerthr;

    /* initializam structura de configurare */
    config_init(&cfg);

    /* incercam sa citim configuratia din fisier */
    if (!config_read_file(&cfg, "server.cfg")) {
        fprintf(stderr, "Eroare la citire config: %s la linia %d\n",
                config_error_text(&cfg), config_error_line(&cfg));
        config_destroy(&cfg);
        return -1;
    }

    /* citim portul serverului Internet din configuratie */
    if (!config_lookup_int(&cfg, "server_config.port", &iport)) {
        fprintf(stderr, "Avertisment: Nu am gasit portul, folosesc default 18081\n");
        iport = 18081;
    } else {
        fprintf(stderr, "[CONFIG] Serverul va porni pe portul %d\n", iport);
    }

    /* stergem vechiul socket UNIX, daca exista deja */
    unlink(UNIXSOCKET);

    /* pornim firul pentru socketul UNIX */
    pthread_create(&unixthr, NULL, unix_main, UNIXSOCKET);

    /* pornim firul pentru socketul Internet */
    pthread_create(&inetthr, NULL, inet_main, &iport);

    /* setam si pornim firul SOAP */
    sport = SOAPPORT;
    pthread_create(&soapthr, NULL, soap_main, &sport);

    /*
     * acest fir ia imaginile din coada
     * si le trimite mai departe la procesare
     */
    pthread_create(&workerthr, NULL, worker_main, NULL);

    /* asteptam terminarea firelor principale */
    pthread_join(unixthr, NULL);
    pthread_join(inetthr, NULL);
    pthread_join(soapthr, NULL);

    /* la final stergem socketul UNIX si eliberam configuratia */
    unlink(UNIXSOCKET);
    config_destroy(&cfg);

    return 0;
}

void *worker_main (void *args) {
    (void)args;

    /* mesaj de pornire pentru firul worker */
    fprintf(stderr, "[WORKER] Firul de procesare a pornit si asteapta poze...\n");

    while (1) {
        int rc;
        ImageTask task = pop_task();

        /* afisam informatii despre taskul extras din coada */
        fprintf(stderr,
                "[WORKER] Am extras o poza din coada. Client=%d, fisier=%s, dimensiune=%zu bytes\n",
                task.client_id,
                task.filename != NULL ? task.filename : "(fara_nume)",
                task.size);

        /* apelam functia care proceseaza imaginea */
        rc = process_image_task(&task);

        /* afisam daca procesarea a reusit sau nu */
        if (rc == 0) {
            fprintf(stderr,
                    "[WORKER] Procesare finalizata cu succes pentru %s\n",
                    task.filename != NULL ? task.filename : "(fara_nume)");
        } else {
            fprintf(stderr,
                    "[WORKER] Procesarea a esuat pentru %s\n",
                    task.filename != NULL ? task.filename : "(fara_nume)");
        }

        /* eliberam memoria asociata taskului */
        free_image_task(&task);
    }

    return NULL;
}
