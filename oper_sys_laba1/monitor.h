//заголовочный файл монитора
#ifndef MONITOR_H
#define MONITOR_H

#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex; // инструмент блокировки доступа
    pthread_cond_t condition; // возможность остановить поток
    int ready; // формальный критерий события
    void *data; // разделяемые данные (указатель на класс)
} Monitor;

void monitor_init(Monitor *m);
void monitor_destroy(Monitor *m);

#endif