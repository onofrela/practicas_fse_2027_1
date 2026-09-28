#include <stdio.h>
#include <pthread.h>

#define ITER 1000000

long contador = 0;
pthread_mutex_t candado = PTHREAD_MUTEX_INITIALIZER;  // 1. declarar

void *sumar(void *arg) {
    for (int i = 0; i < ITER; i++) {
        pthread_mutex_lock(&candado);    // 2. bloquear
        contador++;                       //    sección crítica
        pthread_mutex_unlock(&candado);  // 3. desbloquear
    }
    return NULL;
}

int main(void) {
    pthread_t h1, h2;
    pthread_create(&h1, NULL, sumar, NULL);
    pthread_create(&h2, NULL, sumar, NULL);
    pthread_join(h1, NULL);
    pthread_join(h2, NULL);
    printf("Esperado: %d Obtenido: %ld\n", 2 * ITER, contador);
    pthread_mutex_destroy(&candado);     // 4. destruir
    return 0;
}
