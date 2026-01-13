// Per-thread array index passed via argument.
#include <stdlib.h>
#include <pthread.h>
extern void abort(void);
int *datas;
void assume_abort_if_not(int cond) {
  if(!cond) {abort();}
}


void *thread(void *arg) {
  int i = (int)arg;
  datas[i] = rand(); // NORACE
  return NULL;
}

int main() {
  int threads_total = rand() % 128;
  assume_abort_if_not(threads_total >= 0);

  pthread_t *tids = malloc(threads_total * sizeof(pthread_t));
  datas = malloc(threads_total * sizeof(int));

  // create threads
  for (int i = 0; i < threads_total; i++) {
    pthread_create(&tids[i], NULL, &thread, (void*)i); // may fail but doesn't matter
  }

  // join threads
  for (int i = 0; i < threads_total; i++) {
    pthread_join(tids[i], NULL);
  }

  free(tids);
  free(datas);

  return 0;
}
