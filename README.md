# OS ИДЗ №2
# Селезнев Григорий Ильич вариант №5

## Условие задачи

Задача об обедающих философах. 
Пять философов сидят возле круглого стола. Они проводят жизнь, чередуя приемы пищи и размышления. В центре стола находится большое блюдо спагетти. Спагетти длинные и запутанные, философам тяжело управляться с ними, поэтому каждый из них, что бы съесть порцию, должен пользоваться двумя вилками. К несчастью, философам дали только пять вилок. Между каждой парой философов лежит одна вилка, поэтому эти высококультурные и предельно вежливые люди договорились, что каждый будет пользоваться только теми вилками, которые лежат рядом с ним (слева и справа). Написать программу, моделирующую поведение философов Программа должна избегать фатальной ситуации, в которой все философы голодны, но ни один из них не может взять обе вилки (например, каждый из философов держит по одной вилки и не хочет отдавать ее). Решение должно быть симметричным, то есть все процессы–философы должны выполнять один и тот же код.

### Сценарий:
1. Изначально есть 2 массива сущностей: Вилки и Философы (5 штук каждого)
2. При старте есть 6 процессов: процесс вывода информации происходящего, 5 процессов философов.
3. Процесс вывода информации выводит данные каждые 3 секунды (при этом заходя в крит. секцию)
4. Каждый филосов изначально думает (размышляет) после чего проверяет есть ли на столе хотя бы 2 свободные вилки (перед этим захватив семафор, то есть входит в критическую секцию), после проверяет может ли он взять две вилки (справа и слева от себя). Если так, то он резервирует эти две вилки (помечает их своим айди), иначе он продолжает дальше размышлять.
5. Если Филосов уже взял вилки, то он должен поесть спагетти и отдать обратно вилки (помечает вилки -1, как свободная вилка)
6. Каждый философ должен покушать 3 раза (по умолчанию, можно при запуске указать сколько нужно покушать каждому философу)

## На оценку 4:
> Множество процессов взаимодействуют с использованием именованных POSIX семафоров. Обмен данными ведется через разделяемую память в стандарте POSIX.

> Если программа, которая находится в исходных файлах не работает, то сборка происходила с помощью команды *g++ main.c -o main -pthread*

Программу можно запустить двумя разными способами: *'./main'* и *'./main number'*. В первом случае программа запустятиться и философы должны будут покушать 3 раза, во втором случае программа запуститься и философы должны будут покушать *number* раз

* [Исходные файлы](https://github.com/Grisha1232/OS_IDZ2/tree/main/4)
* [Вывод программы](https://github.com/Grisha1232/OS_IDZ2/blob/main/4/output_4.md)

Программа инициализирует именной семафор, а также разделяемую память. Программа реагирует на поступающие сигналы: *SIGINT* и *SIGTERM* и корректно завершается, то есть закрывает открытый семафор и отвязывает его, а также удаляет разделяемую память

## На оценку 5:
> Множество процессов взаимодействуют с использованием неименованных POSIX семафоров расположенных в разделяемой памяти. Обмен данными также ведется через разделяемую па- мять в стандарте POSIX.

> Если программа, которая находится в исходных файлах не работает, то сборка происходила с помощью команды *g++ main.c -o main -pthread*

* [Исходные файлы](https://github.com/Grisha1232/OS_IDZ2/tree/main/5)
* [Вывод программы](https://github.com/Grisha1232/OS_IDZ2/blob/main/5/output_5.md)

Программа инициализирует безымянный семафор, а также разделяемую память. Программа реагирует на поступающие сигналы: *SIGINT* и *SIGTERM* и корректно завершается, то есть закрывает открытый семафор, а также удаляет разделяемую память

## На оценку 6:
> Множество процессов взаимодействуют с использованием се- мафоров в стандарте UNIX SYSTEM V. Обмен данными ве- дется через разделяемую память в стандарте UNIX SYSTEM V.

> Если программа, которая находится в исходных файлах не работает, то сборка происходила с помощью команды *g++ main.c -o main -pthread*

* [Исходные файлы](https://github.com/Grisha1232/OS_IDZ2/tree/main/6)
* [Вывод программы](https://github.com/Grisha1232/OS_IDZ2/blob/main/6/output_6.md)

Программа инициализирует семафор (SYSTEM V), а также разделяемую память. Программа реагирует на поступающие сигналы: *SIGINT* и *SIGTERM* и корректно завершается, то есть закрывает открытый семафор, а также удаляет разделяемую память


## На оценку 7:
> Множество независимых процессов взаимодействуют с использованием именованных POSIX семафоров. Обмен данными ве- дется через разделяемую память в стандарте POSIX.

> Запуск программы осуществляется через командную строку в разных терминалах:
```
g++ philosophers.c -o phil
./phil
```
```
g++ output.c -o output
./output
```

* [Исходные файлы](https://github.com/Grisha1232/OS_IDZ2/tree/main/6)
* [Вывод программы](https://github.com/Grisha1232/OS_IDZ2/blob/main/6/output_6.md)

Программа инициализирует семафор (SYSTEM V), а также разделяемую память. Программа реагирует на поступающие сигналы: *SIGINT* и *SIGTERM* и корректно завершается, то есть закрывает открытый семафор, а также удаляет разделяемую память

