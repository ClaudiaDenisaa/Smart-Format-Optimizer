/**
 * IR3 2026
 * Programul defineste structurile si functiile necesare
 * pentru gestionarea unei cozi de taskuri cu imagini.
 *
 * Fisierul contine:
 * - structura unui task de procesare imagine
 * - structura unui nod din coada
 * - prototipurile functiilor pentru adaugare, extragere
 *   si eliberare a unui task
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>   /* utilizat pentru: size_t */
#include <pthread.h>  /* utilizat pentru tipurile pthread */

/* structura care retine informatiile necesare pentru o imagine */
typedef struct {
    unsigned char *data; /* bufferul cu datele imaginii */
    size_t size;         /* dimensiunea imaginii in octeti */
    int socket;          /* socketul clientului care a trimis imaginea */
    int client_id;       /* id-ul clientului */
    char *filename;      /* numele fisierului imagine */
} ImageTask;

/* nod din coada de taskuri */
typedef struct Node {
    ImageTask task;     /* taskul stocat in nod */
    struct Node *next;  /* legatura catre urmatorul nod */
} Node;

/* adauga un task la finalul cozii */
void push_task(ImageTask t);

/* extrage primul task din coada */
ImageTask pop_task(void);

/* elibereaza memoria folosita de un task */
void free_image_task(ImageTask *task);

#endif