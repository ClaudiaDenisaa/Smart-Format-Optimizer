#include "queue.h"
#include <stdlib.h>

static Node *head = NULL;
static pthread_mutex_t q_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t q_cond = PTHREAD_COND_INITIALIZER;

void push_task(ImageTask t) {
    pthread_mutex_lock(&q_mtx);
    Node *n = malloc(sizeof(Node));
    n->task = t;
    n->next = head;
    head = n;
    pthread_cond_signal(&q_cond); // Trezește firul Patriciei
    pthread_mutex_unlock(&q_mtx);
}

ImageTask pop_task() {
    pthread_mutex_lock(&q_mtx);
    while (head == NULL) pthread_cond_wait(&q_cond, &q_mtx);
    Node *tmp = head;
    ImageTask t = tmp->task;
    head = head->next;
    free(tmp);
    pthread_mutex_unlock(&q_mtx);
    return t;
}
