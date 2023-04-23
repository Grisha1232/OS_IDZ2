#include <fcntl.h>
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
#define SHAR_OBJ "/posix-shar1"     // Имя памяти

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

sem_t *sem;
sem_t *admin;
int fd;
shared_memory *buffer;

void printMemory() {
  printf("----------------------------------------\n");
  for (int i = 0; i < COUNT; i++) {
    if (buffer->forks[i].isUsingBy == -1) {
      printf("|\tfork %d is free\t\t\t|\n", buffer->forks[i].id);
    } else {
      printf("|\tfork %d is using by philosopher %d|\n", buffer->forks[i].id, buffer->forks[i].isUsingBy);
    }
  }
  printf("----------------------------------------\n");
  for (int i = 0; i < COUNT; i++) {
    printf("|\tphil %d has ate %d times\t\t|\n", i, buffer->phil[i].count_of_meals);
  }
  printf("----------------------------------------\n");
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

  if (sem_close(sem) == -1) {
    perror("sem_close");
    exit(-1);
  }

  if (sem_close(admin) == -1) {
    perror("sem_close");
    exit(1);
  }
  exit(10);
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sigfunc);
  signal(SIGTERM, sigfunc);

  pid_t pid;

  // Создаем разделяемую память
  fd = shm_open(SHAR_OBJ, O_CREAT | O_RDWR, 0666);
  if (fd == -1) {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }

  ftruncate(fd, sizeof(shared_memory));


  // ! Создаем именованнный семафор для синхронизации процессов !
  buffer = (shared_memory *) mmap(NULL, sizeof(shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (buffer == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  printf("open shared memory\n");


  sem = sem_open(NAME_SEM, O_CREAT | O_EXCL, 0666, 1);
  if (sem == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  admin = sem_open(NAME_ADMIN, O_CREAT, 0666, 0);
  if (admin == SEM_FAILED) {
    perror("sem_open");
    exit(1);
  }

  printf("make semaphores\n");

  // Инициализируем состояние стола
  for (int i = 0; i < COUNT; i++) {
    buffer->forks[i].id = i;
    buffer->forks[i].isUsingBy = -1;
    buffer->available = 5;
    buffer->phil[i].id = i;
    buffer->phil[i].count_of_meals = 0;
  }
  buffer->min_count_of_meal = 0;
  buffer->count_of_meal_needed = 3;
  if (argc == 2) {
    buffer->count_of_meal_needed = atoi(argv[1]);
  }
  buffer->output_pid = getpid();
  printf("init end\n");
  sem_post(admin);
  printf("post admin semaphore\n");
  int ret = 0;
  sem_getvalue(admin, &ret);
  printf("adim = %d\n", ret);

  while (buffer->min_count_of_meal != buffer->count_of_meal_needed) {
    sleep(5);
    sem_wait(sem);
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
    sem_post(sem);
  }

  // Удаляем семафор и разделяемую память
  sem_unlink(NAME_SEM);
  sem_close(sem);
  shm_unlink(NAME_SEM);
  munmap(buffer, sizeof(shared_memory));
  close(fd);

  return 0;
}
