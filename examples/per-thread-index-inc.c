// Per-thread array index using counter increment.
// Extracted from concrat/ProcDump-for-Linux.
#include <stdlib.h>
#include <pthread.h>
#include <strings.h>
extern void abort(void);
void assume_abort_if_not(int cond) {
  if(!cond) {abort();}
}

int *datas;

int next_j = 0;
pthread_mutex_t next_j_mutex = PTHREAD_MUTEX_INITIALIZER;

void *thread(void *arg) {
  int j;
  pthread_mutex_lock(&next_j_mutex);
  j = next_j; // NORACE
  next_j++; // NORACE
  pthread_mutex_unlock(&next_j_mutex);

  datas[j] = rand(); // NORACE
  return NULL;
}

int main() {
  int threads_total = rand() % 128;
  assume_abort_if_not(threads_total >= 0);

  pthread_t *tids = malloc(threads_total * sizeof(pthread_t));
  datas = malloc(threads_total * sizeof(int));

  // create threads
  for (int i = 0; i < threads_total; i++) {
    pthread_create(&tids[i], NULL, &thread, NULL); // may fail but doesn't matter
  }

  // join threads
  for (int i = 0; i < threads_total; i++) {
    pthread_join(tids[i], NULL);
  }

  free(tids);
  free(datas);

  return 0;
}
