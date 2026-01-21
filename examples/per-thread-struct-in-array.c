// Per-thread structs in array passed via argument.
// Extracted from concrat/ProcDump-for-Linux.
#include <stdlib.h>
#include <pthread.h>
extern void abort(void);
void assume_abort_if_not(int cond) {
  if(!cond) {abort();}
}

struct thread {
  int data;
};

void *thread(void *arg) {
  struct thread *t = arg;
  t->data = rand(); // NORACE
  return NULL;
}

int main() {
  int threads_total = rand() % 128;
  assume_abort_if_not(threads_total >= 0);

  pthread_t *tids = malloc(threads_total * sizeof(pthread_t));
  struct thread *ts = malloc(threads_total * sizeof(struct thread));

  // create threads
  for (int i = 0; i < threads_total; i++) {
    pthread_create(&tids[i], NULL, &thread, &ts[i]); // may fail but doesn't matter
  }

  // join threads
  for (int i = 0; i < threads_total; i++) {
    pthread_join(tids[i], NULL);
  }

  free(tids);
  free(ts);

  return 0;
}
