// Множество процессов взаимодействуют с использованием
// именованных POSIX семафоров. Обмен данными ведется через
// разделяемую память в стандарте POSIX.
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define COUNT 5

#define NAME_SEM "/posix_semaphore"// Имя семафора
#define SHAR_OBJ "/posix-shar"     // Имя памяти

int count_of_meals_needed = 3;// дефолтное значение сколько раз должен каждый философ поесть
int min_count_of_meals = 0;   // сколько минимально съел каждый философ

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
} shared_memory;

sem_t *sem;
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

void sigfunc(int sig) {
  if (sig != SIGINT && sig != SIGTERM) {
    return;
  }

  // Закрываем всё
  sem_unlink(NAME_SEM);
  sem_close(sem);
  shm_unlink(SHAR_OBJ);
  munmap(buffer, sizeof(shared_memory));
  close(fd);

  printf("Sig finished\n");
  exit(10);
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sigfunc);
  signal(SIGTERM, sigfunc);

  printf("Many processes interact using NAMED POSIX semaphores located in shared memory.\n");

  pid_t pid;

  // Проверка: Удалилась ли память в прошлый раз
  if (shm_unlink(SHAR_OBJ) != -1) {
    perror("shm_unlink");
  }

  // Создаем разделяемую память
  fd = shm_open("SHM_NAME", O_CREAT | O_RDWR, 0666);
  if (fd == -1) {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }

  ftruncate(fd, sizeof(shared_memory));

  // Проверка: Удалился ли семафор в прошлый раз
  if (sem_unlink(NAME_SEM) != -1) {
    perror("sem_unlink");
  }

  // ! Создаем именованнный семафор для синхронизации процессов !
  buffer = (shared_memory *) mmap(NULL, sizeof(shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (buffer == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  sem = sem_open(NAME_SEM, O_CREAT | O_EXCL, 0666, 1);
  if (sem == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  // Инициализируем состояние стола
  for (int i = 0; i < COUNT; i++) {
    buffer->forks[i].id = i;
    buffer->forks[i].isUsingBy = -1;
    buffer->available = 5;
    buffer->phil[i].id = i;
    buffer->phil[i].count_of_meals = 0;
  }

  // Чтение входных данных (сколько раз должны поесть философы)
  if (argc == 2) {
    count_of_meals_needed = atoi(argv[1]);
  }

  // Создаем все процессы

  pid = fork();
  if (pid == -1) {
    perror("fork");
    exit(1);
  } else if (pid != 0) {
    // процесс вилок (будет выводить какие вилки взяты каждые 5 секунды)
    while (min_count_of_meals != count_of_meals_needed) {
      sleep(5);
      sem_wait(sem);
      printMemory();
      int min = 10000;
      for (int i = 0; i < COUNT; i++) {
        if (min > buffer->phil[i].count_of_meals) {
          min = buffer->phil[i].count_of_meals;
        }
      }
      if (min > min_count_of_meals) {
        min_count_of_meals = min;
      }
      sem_post(sem);
    }
    exit(0);
  } else {
    // процесс философов (будет выводить кто сейчас ест)
    pid_t phil = fork();
    if (phil == -1) {
      perror("fork");
    } else if (phil == 0) {
      pid_t another_phil = fork();
      if (another_phil == -1) {
        perror("fork");
      } else if (another_phil == 0) {// первый
        while (min_count_of_meals != count_of_meals_needed && buffer->phil[0].count_of_meals < count_of_meals_needed) {
          if (buffer->phil[0].isUsingFork == 1) {
            printf("phil 0 is eating\n");
          } else {
            printf("phil 0 is thinking\n");
          }
          sleep(rand() % 5 + 1);
          if (buffer->phil[0].isUsingFork == 1) {
            funcPhil_unreserveForks(buffer, 0);
          } else {
            funcPhil_prepareForEating(buffer, 0);
          }
        }
        exit(0);
      } else {
        pid_t last = fork();
        if (last == -1) {
          perror("fork");
          exit(1);
        } else if (last == 0) {// второй
          while (min_count_of_meals != count_of_meals_needed && buffer->phil[1].count_of_meals < count_of_meals_needed) {
            if (buffer->phil[1].isUsingFork == 1) {
              printf("phil 1 is eating\n");
            } else {
              printf("phil 1 is thinking\n");
            }
            sleep(rand() % 5 + 1);
            if (buffer->phil[1].isUsingFork == 1) {
              funcPhil_unreserveForks(buffer, 1);
            } else {
              funcPhil_prepareForEating(buffer, 1);
            }
          }
          exit(0);
        } else {// третий
          while (min_count_of_meals != count_of_meals_needed && buffer->phil[2].count_of_meals < count_of_meals_needed) {
            if (buffer->phil[2].isUsingFork == 1) {
              printf("phil 2 is eating\n");
            } else {
              printf("phil 2 is thinking\n");
            }
            sleep(rand() % 5 + 1);
            if (buffer->phil[2].isUsingFork == 1) {
              funcPhil_unreserveForks(buffer, 2);
            } else {
              funcPhil_prepareForEating(buffer, 2);
            }
          }
          exit(0);
        }
      }
    } else {
      pid_t another_phil = fork();
      if (another_phil == -1) {
        perror("fork");
        exit(1);
      } else if (another_phil == 0) {// четвертый
        while (min_count_of_meals != count_of_meals_needed && buffer->phil[3].count_of_meals < count_of_meals_needed) {
          if (buffer->phil[3].isUsingFork == 1) {
            printf("phil 3 is eating\n");
          } else {
            printf("phil 3 is thinking\n");
          }
          sleep(rand() % 5 + 1);
          if (buffer->phil[3].isUsingFork == 1) {
            funcPhil_unreserveForks(buffer, 3);
          } else {
            funcPhil_prepareForEating(buffer, 3);
          }
        }
        exit(0);
      } else {// пятый
        while (min_count_of_meals != count_of_meals_needed && buffer->phil[4].count_of_meals < count_of_meals_needed){
          if (buffer->phil[4].isUsingFork == 1) {
            printf("phil 4 is eating\n");
          } else {
            printf("phil 4 is thinking\n");
          }
          sleep(rand() % 5 + 1);
          if (buffer->phil[4].isUsingFork == 1) {
            funcPhil_unreserveForks(buffer, 4);
          } else {
            funcPhil_prepareForEating(buffer, 4);
          }
        }
        exit(0);
      }
    }
  }

  int status;
  waitpid(pid, &status, 0);

  // Удаляем семафор и разделяемую память
  sem_unlink(NAME_SEM);
  sem_close(sem);
  shm_unlink(NAME_SEM);
  munmap(buffer, sizeof(shared_memory));
  close(fd);

  return 0;
}
