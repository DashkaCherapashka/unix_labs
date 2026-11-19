#include <stdio.h>
#include "monitor.h"

int main(void) {
    Monitor m;
    monitor_init(&m);

    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer, &m);
    pthread_create(&cons, NULL, consumer, &m);

    // дождаться потоков, потом уничтожать монитор
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    monitor_destroy(&m);
    return 0;
}