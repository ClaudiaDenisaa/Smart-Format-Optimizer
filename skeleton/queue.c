/**
 * IR3 2026
 * Programul realizeaza operatii pentru o coada de taskuri cu imagini.
 *
 * Sunt implementate functii pentru:
 * - adaugarea unui task in coada
 * - extragerea unui task din coada
 * - eliberarea memoriei asociate unui task
 *
 * Coada este protejata cu mutex si condition variable,
 * pentru a putea fi folosita in siguranta de mai multe threaduri.
 */

#include "queue.h"     /* structurile Node si ImageTask, plus prototipurile functiilor */

#include <stdlib.h>    /* utilizat pentru: malloc, free */
#include <string.h>    /* utilizat pentru: memset */

/* inceputul cozii */
static Node *head = NULL;

/* sfarsitul cozii */
static Node *tail = NULL;

/* mutex pentru acces sincronizat la coada */
static pthread_mutex_t q_mtx = PTHREAD_MUTEX_INITIALIZER;

/* variabila de conditie folosita cand coada este goala */
static pthread_cond_t q_cond = PTHREAD_COND_INITIALIZER;

void push_task(ImageTask t) {
    Node *n;

    /* aloca un nod nou pentru taskul primit */
    n = (Node *)malloc(sizeof(Node));
    if (n == NULL) {
        return;
    }

    /* copiem taskul in nod si marcam finalul listei */
    n->task = t;
    n->next = NULL;

    /* blocam accesul la coada cat timp facem modificari */
    pthread_mutex_lock(&q_mtx);

    /* daca lista este goala, noul nod devine si head si tail */
    if (tail == NULL) {
        head = n;
        tail = n;
    } else {
        /* altfel il adaugam la final */
        tail->next = n;
        tail = n;
    }

    /* anuntam un posibil thread care asteapta un task */
    pthread_cond_signal(&q_cond);

    /* eliberam mutexul */
    pthread_mutex_unlock(&q_mtx);
}

ImageTask pop_task(void) {
    Node *tmp;
    ImageTask t;

    /* initializam taskul cu 0, pentru siguranta */
    memset(&t, 0, sizeof(t));

    /* blocam accesul la coada */
    pthread_mutex_lock(&q_mtx);

    /* daca nu exista taskuri, threadul asteapta */
    while (head == NULL) {
        pthread_cond_wait(&q_cond, &q_mtx);
    }

    /* luam primul nod din coada */
    tmp = head;
    t = tmp->task;
    head = head->next;

    /* daca am scos ultimul element, resetam si tail */
    if (head == NULL) {
        tail = NULL;
    }

    /* eliberam nodul scos din coada */
    free(tmp);

    /* eliberam mutexul */
    pthread_mutex_unlock(&q_mtx);

    return t;
}

void free_image_task(ImageTask *task) {
    /* verificam daca pointerul primit este valid */
    if (task == NULL) {
        return;
    }

    /* eliberam bufferul cu datele imaginii */
    free(task->data);
    task->data = NULL;

    /* eliberam numele fisierului */
    free(task->filename);
    task->filename = NULL;

    /* resetam restul campurilor */
    task->size = 0;
    task->socket = -1;
    task->client_id = 0;
}