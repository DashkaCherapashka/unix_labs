// поток потребитель
#include <stdio.h>
#include <stdlib.h>
#include "monitor.h"

#define EVENTS_COUNT 5

void *consumer(void *arg) {
    Monitor *m = (Monitor *)arg;

    for (int i = 0; i < EVENTS_COUNT; i++) {
        pthread_mutex_lock(&m->mutex);

        while (!m->ready) {
            pthread_cond_wait(&m->condition, &m->mutex);
        }

        int *event = (int *)m->data; 
        m->ready = 0;    

        printf("Потребитель: событие %d обработано\n", *event);
        fflush(stdout);

        free(event);
        m->data = NULL;

        pthread_cond_signal(&m->condition); // разбудить поставщика
        pthread_mutex_unlock(&m->mutex);
    }
    return NULL;
}