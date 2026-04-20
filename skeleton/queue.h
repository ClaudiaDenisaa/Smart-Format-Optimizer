#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

typedef struct {
    unsigned char *data;
    size_t size;
    int socket;
} ImageTask;

typedef struct Node {
    ImageTask task;
    struct Node *next;
} Node;

void push_task(ImageTask t);
ImageTask pop_task();

#endif
