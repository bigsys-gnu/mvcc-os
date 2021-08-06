#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#define NUM 10

pthread_mutex_t m;
pthread_cond_t cv;

void *worker(void *arg)
{
  int id = *(int *)arg;
  /* struct timespec ts = {1000*1000*1000}; */

  pthread_mutex_lock(&m);
  /* pthread_cond_timedwait(&cv, &m, &ts); */
  pthread_cond_wait(&cv, &m);
  printf("%d wake\n", id);
  pthread_mutex_unlock(&m);
  printf("%d done\n", id);

  return NULL;
}

int main(int argc, char *argv[])
{
  int i;
  int arg_list[NUM];
  pthread_t tid_list[NUM];

  pthread_mutex_init(&m, NULL);
  pthread_cond_init(&cv, NULL);

  for (i = 0; i < NUM; i++)
  {
    arg_list[i] = i;
    xthread_create(&tid_list[i], 0, worker, &arg_list[i]);
  }

  sleep(5);
  printf("timeout\n");
  pthread_cond_broadcast(&cv);

  for (i = 0; i < NUM; i++)
  {
    pthread_join(tid_list[i], NULL);
  }

  printf("main done\n");
  
  return 0;
}
