//инициализация и уничтожение монитора
#include "monitor.h"

void monitor_init(Monitor *m) {
    pthread_mutex_init(&m->mutex, NULL);
    pthread_cond_init(&m->condition, NULL);
    m->ready = 0;
    m->data  = NULL;
}

// Вызывать только после pthread_join обоих потоков
void monitor_destroy(Monitor *m) {
    pthread_mutex_destroy(&m->mutex);
    pthread_cond_destroy(&m->condition);
}