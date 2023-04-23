#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define COUNT 5

#define NAME_SEM "/posix_semaphore1"// Имя семафора
#define NAME_ADMIN "/posix-admin1"
#define SHAR_OBJ "/posix-shar1"// Имя памяти

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

sem_t *sem;
sem_t *admin;
int fd;
shared_memory *buffer;

void sigfunc(int sig) {
  if (sig != SIGINT && sig != SIGTERM) {
    return;
  }
  if (sig == SIGINT) {
    kill(buffer->output_pid, SIGTERM);
    printf("Reader(SIGINT) ---> Writer(SIGTERM)\n");
  } else if (sig == SIGTERM) {
    printf("Reader(SIGTERM) <--- Writer(SIGINT)\n");
  }

  if (sem_close(sem) == -1) {
    perror("sem_close");
    exit(-1);
  }

  if (sem_close(admin) == -1) {
    perror("sem_close");
    exit(1);
  }
  sem_unlink(NAME_ADMIN);
  sem_unlink(NAME_SEM);
  exit(10);
}

void funcPhil_prepareForEating(shared_memory *buf, int id) {
  sem_wait(sem);
  if (buf->available >= 2) {
    if (buf->forks[id].isUsingBy == -1 && buf->forks[(id + 1) % COUNT].isUsingBy == -1) {
      buf->available -= 2;
      buf->forks[id].isUsingBy = id;
      buf->forks[(id + 1) % COUNT].isUsingBy = id;
      buf->phil[id].isUsingFork = 1;
      printf("phil %d taking forks\n", id);
    }
  }
  sem_post(sem);
  sleep(2);
}

void funcPhil_unreserveForks(shared_memory *buf, int id) {
  sem_wait(sem);
  buf->available += 2;
  buf->forks[id].isUsingBy = -1;
  buf->forks[(id + 1) % COUNT].isUsingBy = -1;
  buf->phil[id].count_of_meals++;
  buf->phil[id].isUsingFork = 0;
  printf("phil %d has ate\n", id);
  sem_post(sem);
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

  admin = sem_open(NAME_ADMIN, O_CREAT, 0666, 0);
  if (admin == SEM_FAILED) {
    perror("sem_open");
    exit(1);
  }

  printf("waiting for adim\n");
  sem_wait(admin);

  // Создаем разделяемую память
  fd = shm_open(SHAR_OBJ, O_CREAT | O_RDWR, 0666);
  if (fd == -1) {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }

  ftruncate(fd, sizeof(shared_memory));

  printf("ftruncate\n");

  // Проверка: Удалился ли семафор в прошлый раз
  sem_unlink(NAME_SEM);
  sem_unlink(NAME_ADMIN);

  // ! Создаем именованнный семафор для синхронизации процессов !
  buffer = (shared_memory *) mmap(NULL, sizeof(shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (buffer == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  printf("shared memory\n");

  sem = sem_open(NAME_SEM, O_CREAT | O_EXCL, 0666, 1);
  if (sem == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  printf("semaphores open\n");
  printf("start\n");
  buffer->philosopher_pid = getpid();

  pthread_t pt[COUNT];
  for (int i = 0; i < COUNT; i++) {
    A *arg;
    arg = (A *) malloc(sizeof(A));
    arg->buf = buffer;
    arg->id = i;
    pthread_create(&pt[i], NULL, startPhil, arg);
  }

  for (int i = 0; i < COUNT; i++) {
    pthread_join(pt[i], NULL);
  }
  // Удаляем семафор и разделяемую память
  sem_unlink(NAME_SEM);
  sem_unlink(NAME_ADMIN);
  sem_close(sem);
  sem_close(admin);
  munmap(buffer, sizeof(shared_memory));
  close(fd);

  return 0;
}
