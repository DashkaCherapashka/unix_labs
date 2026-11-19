// поток поставщик
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "monitor.h"

#define EVENTS_COUNT 5

void *producer(void *arg) {
    Monitor *m = (Monitor *)arg;

    for (int i = 0; i < EVENTS_COUNT; i++) {
        sleep(1); // задержка 1 секунда

        int *event = malloc(sizeof(int));
        if (!event) return NULL;
        *event = i;

        pthread_mutex_lock(&m->mutex);

        // ждём, пока предыдущее событие не обработано
        while (m->ready) {
            pthread_cond_wait(&m->condition, &m->mutex);
        }

        m->data  = event; 
        m->ready = 1; 

        printf("Поставщик: событие %d отправлено\n", i);
        fflush(stdout);

        pthread_cond_signal(&m->condition); // разбудить потребителя
        pthread_mutex_unlock(&m->mutex);
    }
    return NULL;
}