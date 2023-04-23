#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#define COUNT 5
#define KEY 9090

typedef struct {
  int id;       // id вилки
  int isUsingBy;// кем используется (-1 для свободной)
} Fork;

typedef struct {
  int id;
  int count_of_meals;
  int isUsingFork;// 0 для нет 1 для да
} Philosopher;

// Структура, хранящая информацию о состоянии клумбы и днях
typedef struct {
  Fork forks[COUNT];// все вилки на столе
  int available;    // свободные вилки
  Philosopher phil[COUNT];
  pid_t philosopher_pid;
  pid_t output_pid;
  int min_count_of_meal;   // сколько минимально съел каждый философ
  int count_of_meal_needed;// дефолтное значение сколько раз должен каждый философ поесть

} shared_memory;

typedef struct {
  shared_memory *buf;
  int id;
} A;

struct sembuf lock = {0, -1, SEM_UNDO};      // операция убавление единицы
struct sembuf unlock = {0, 1, SEM_UNDO};     // операция добавление единицы
struct sembuf lockAdmin = {1, -1, SEM_UNDO}; // операция убавления единицы
struct sembuf unlockAdmin = {1, 1, SEM_UNDO};// операция добавления единицы

int sem_v;
int buf_id;
shared_memory *buffer;


void sigfunc(int sig) {
  if (sig != SIGINT && sig != SIGTERM) {
    return;
  }
  if (sig == SIGINT) {
    kill(buffer->philosopher_pid, SIGTERM);
    printf("Reader(SIGINT) ---> Writer(SIGTERM)\n");
  } else if (sig == SIGTERM) {
    printf("Reader(SIGTERM) <--- Writer(SIGINT)\n");
  }

  semctl(sem_v, 0, IPC_RMID);
  shmctl(buf_id, IPC_RMID, NULL);
  exit(10);
}

void funcPhil_prepareForEating(shared_memory *buf, int id) {
  semop(sem_v, &lock, 1);
  if (buf->available >= 2) {
    if (buf->forks[id].isUsingBy == -1 && buf->forks[(id + 1) % COUNT].isUsingBy == -1) {
      buf->available -= 2;
      buf->forks[id].isUsingBy = id;
      buf->forks[(id + 1) % COUNT].isUsingBy = id;
      buf->phil[id].isUsingFork = 1;
      printf("phil %d taking forks\n", id);
    }
  }
  semop(sem_v, &unlock, 1);
  sleep(2);
}

void funcPhil_unreserveForks(shared_memory *buf, int id) {
  semop(sem_v, &lock, 1);
  buf->available += 2;
  buf->forks[id].isUsingBy = -1;
  buf->forks[(id + 1) % COUNT].isUsingBy = -1;
  buf->phil[id].count_of_meals++;
  buf->phil[id].isUsingFork = 0;
  printf("phil %d has ate\n", id);
  semop(sem_v, &unlock, 1);
}

void *startPhil(void *args) {
  A *ar = (A *) args;
  int id = ar->id;
  shared_memory *buff = ar->buf;
  while (buff->phil[id].count_of_meals < buff->count_of_meal_needed) {
    if (buff->phil[id].isUsingFork == 1) {
      printf("phil %d is eating\n", id);
    } else {
      printf("phil %d is thinking\n", id);
    }
    sleep(rand() % 5 + 1);
    if (buff->phil[id].isUsingFork == 1) {
      funcPhil_unreserveForks(buffer, id);
    } else {
      funcPhil_prepareForEating(buffer, id);
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sigfunc);
  signal(SIGTERM, sigfunc);

  pid_t pid;

  // инициализируем system V semaphore
  if ((sem_v = semget(KEY, 2, IPC_CREAT | 0666)) == -1) {
    perror("shmget");
    exit(1);
  }

  semop(sem_v, &lockAdmin, 1);

  // Создаем разделяемую память
  if ((buf_id = shmget(KEY, sizeof(shared_memory), IPC_CREAT | 0666)) == -1) {
    perror("shmget buf_id");
    exit(1);
  }

  // ! Создаем именованнный семафор для синхронизации процессов !
  buffer = (shared_memory *) shmat(buf_id, 0, 0);
  if (buffer == (shared_memory *) -1) {
    perror("shmat");
    exit(1);
  }

  printf("init end\n");

  pthread_t pt[COUNT];
  for (int i = 0; i < COUNT; i++) {
    A *ar = (A *) malloc(sizeof (A));
    ar->id = i;
    ar->buf = buffer;
    pthread_create(&pt[i], NULL, startPhil, ar);
  }

  for (int i = 0; i < COUNT; i++) {
    pthread_join(pt[i], NULL);
  }
  // Удаляем семафор и разделяемую память
  semctl(sem_v, 0, IPC_RMID);
  shmctl(buf_id, IPC_RMID, NULL);

  return 0;
}
