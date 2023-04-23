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

struct sembuf lock = {0, -1, SEM_UNDO};      // операция убавление единицы
struct sembuf unlock = {0, 1, SEM_UNDO};     // операция добавление единицы
struct sembuf lockAdmin = {1, -1, SEM_UNDO}; // операция убавления единицы
struct sembuf unlockAdmin = {1, 1, SEM_UNDO};// операция добавления единицы

int sem_v;
int buf_id;
shared_memory *buffer;

void printMemory() {
  printf("------------------------------------\n");
  for (int i = 0; i < COUNT; i++) {
    if (buffer->forks[i].isUsingBy == -1) {
      printf("|\tfork %d is free\t\t\t|\n", buffer->forks[i].id);
    } else {
      printf("|\tfork %d is using by philosopher %d|\n", buffer->forks[i].id, buffer->forks[i].isUsingBy);
    }
  }
  printf("-----------------------------------\n");
  for (int i = 0; i < COUNT; i++) {
    printf("|\tphil %d has ate %d times\t\t|\n", i, buffer->phil[i].count_of_meals);
  }
  printf("-----------------------------------\n");
}

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

int main(int argc, char *argv[]) {
  signal(SIGINT, sigfunc);
  signal(SIGTERM, sigfunc);

  pid_t pid;

  printf("starting output.c\n");
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

  // инициализируем system V semaphore
  if ((sem_v = semget(KEY, 2, IPC_CREAT | 0666)) == -1) {
    perror("shmget");
    exit(1);
  }

  semop(sem_v, &unlock, 1);

  // Инициализируем состояние стола
  for (int i = 0; i < COUNT; i++) {
    buffer->forks[i].id = i;
    buffer->forks[i].isUsingBy = -1;
    buffer->available = 5;
    buffer->phil[i].id = i;
    buffer->phil[i].count_of_meals = 0;
    buffer->phil[i].isUsingFork = 0;
  }
  buffer->min_count_of_meal = 0;
  buffer->count_of_meal_needed = 3;
  if (argc == 2) {
    buffer->count_of_meal_needed = atoi(argv[1]);
  }
  buffer->output_pid = getpid();
  printf("init end\n");

  // делаем значение семафору admin 1
  semop(sem_v, &unlockAdmin, 1);

  // процесс вилок (будет выводить какие вилки взяты каждые 5 секунды)
  while (buffer->min_count_of_meal != buffer->count_of_meal_needed) {
    sleep(3);
    semop(sem_v, &lock, 1);
    printMemory();
    int min = 10000;
    for (int i = 0; i < COUNT; i++) {
      if (min > buffer->phil[i].count_of_meals) {
        min = buffer->phil[i].count_of_meals;
      }
    }
    if (min > buffer->min_count_of_meal) {
      buffer->min_count_of_meal = min;
    }
    semop(sem_v, &unlock, 1);
  }

  // Удаляем семафор и разделяемую память
  semctl(sem_v, 0, IPC_RMID);
  shmctl(buf_id, IPC_RMID, NULL);

  return 0;
}
