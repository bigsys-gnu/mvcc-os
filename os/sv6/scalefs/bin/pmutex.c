#include <stdio.h>
#include <unistd.h>
#include <pthread.h>


#define THREAD_NUM 10

pthread_mutex_t m;
int val;

void *worker(void *arg) {
  pthread_mutex_lock(&m);
  val = *(int *)arg;
  printf("%d\n", val);
  sleep(1);
  pthread_mutex_unlock(&m);

  return NULL;
}



int main(int argc, char *argv[])
{
  int i;
  pthread_t tid[THREAD_NUM];
  int arguments[THREAD_NUM];

  pthread_mutex_init(&m, NULL);
  val = -1;

  for (i = 0; i < THREAD_NUM; i++)
  {
    arguments[i] = i;
    pthread_create(&tid[i], NULL, worker, &arguments[i]);
  }

  for (i = 0; i < THREAD_NUM; i++)
  {
    pthread_join(tid[i], NULL);
  }

  return 0;
}

