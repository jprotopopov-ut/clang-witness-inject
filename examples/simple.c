#include <stdlib.h>
#include <pthread.h>

static int value = 0;
static _Atomic int flag = 0;

static void *fn1(void *arg) {
    (void) arg;
    value++;
    flag = 1;
    return NULL;
}

static void *fn2(void *arg) {
    (void) arg;
    for (;;) {
        if (flag) {
            (value)++;
            break;
        }
    }
    return NULL;
}

int main(int argc, const char **argv) {
    pthread_t thr1, thr2;

    pthread_create(&thr1, NULL, fn1, NULL);
    pthread_create(&thr2, NULL, fn2, NULL);

    pthread_join(thr1, NULL);
    pthread_join(thr2, NULL);

    return EXIT_SUCCESS;
}
