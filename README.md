<!-- Главная страница пакета после С1: `make s01-pack-posle` копирует этот файл в корень пакета как README.md, поэтому ссылки даны относительно корня пакета. -->

# OpenMP · Семинар 1: материалы после занятия

Курс «Параллельное программирование на OpenMP». Первый семинар: от MPI и CUDA к общей памяти, модель fork-join, области видимости данных, первая гонка данных.

## Лекция

[Слайды семинара 1 (PDF)](pdf/s01-vvedenie.pdf). Вопросы на слайдах те же, что звучали на занятии, ответ — на следующем слайде. Попробуйте сначала ответить сами.

Числа на слайдах получены на машине преподавателя. У вас времена будут другими, а программы с ошибками (гонка данных, выход за границу массива) могут вести себя иначе, чем на слайдах, — в этом и состоит неопределённое поведение.

## Код и упражнения

- Примеры с занятия: [seminars/s01-vvedenie/code/](seminars/s01-vvedenie/code/). `make` собирает всё, `make info` показывает, какой компилятор выбран, `make run` печатает таблицу T(p) для трёх версий π.
- Упражнения для самопроверки, сдавать не нужно: [seminars/s01-vvedenie/praktika.md](seminars/s01-vvedenie/praktika.md) — повторить демонстрации, сделать блочное разбиение π и замерить ускорение, распараллелить формулу Симпсона.

## ДЗ0 — до начала второго семинара

1. **Установите компилятор с OpenMP** по инструкции [docs/02-okruzhenie.md](docs/02-okruzhenie.md): Linux — gcc из пакетов дистрибутива; Windows — WSL2 + Ubuntu (запасной вариант — MSYS2 UCRT64); macOS — Homebrew и `brew install gcc`. Visual Studio не подходит: её `/openmp` — это OpenMP 2002 года. Если в WSL `sudo apt update` не может подключиться к серверам Ubuntu, решение — в первой строке раздела «Частые проблемы» той же инструкции.
2. **Текст задания:** [homework/hw0-okruzhenie/README.md](homework/hw0-okruzhenie/README.md).
3. **Шаблон личного репозитория:** [templates/student-repo/](templates/student-repo/). Репозиторий создаётся из шаблона на GitHub по ссылке, которую прислал преподаватель.
4. **Скрипты замеров и паспорта стенда:** [tools/](tools/) (те же лежат в шаблоне).

Не получается — пишите в чат курса: полный текст ошибки и вывод `make info`.

> **Windows + WSL.** Распакуйте архив внутри WSL, в домашний каталог, а не в `/mnt/c/…`:
> `sudo apt install unzip && cd ~ && unzip /mnt/c/Users/<имя>/Downloads/openmp-s01-posle.zip && cd openmp-s01-posle`
> (путь к архиву поправьте под себя). Из `/mnt/c` сборка и git работают в разы медленнее.

## Что в архиве

```
README.md                      этот файл
pdf/s01-vvedenie.pdf           слайды лекции
seminars/s01-vvedenie/
  code/                        примеры с занятия: make, make info, make run
  code/practice/               заготовки для упражнений
  praktika.md                  упражнения после занятия (необязательно)
homework/hw0-okruzhenie/       ДЗ0
docs/02-okruzhenie.md          установка OpenMP на Windows / macOS / Linux, как мерить время на ноутбуке
templates/student-repo/        шаблон личного репозитория: Makefile, CI, шаблоны REPORT.md и SOURCES.md
tools/                         bench.py, plot.py, sysinfo.sh, sysinfo.ps1
```
