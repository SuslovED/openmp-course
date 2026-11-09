# Инструменты курса

Все скрипты копируются в репозиторий студента (`templates/student-repo/tools/`). Нужен Python 3.8+, для графиков ещё `matplotlib` (`pip install matplotlib` или `sudo apt install python3-matplotlib`).

| Файл | Назначение |
|---|---|
| `bench.py` | серия замеров программы → CSV (+ `.meta.json` со сведениями о машине) |
| `plot.py` | CSV → графики времени, ускорения, эффективности; markdown-таблицы для отчёта |
| `sysinfo.sh` | паспорт стенда `SYSTEM.md` (Linux, WSL, macOS) |
| `sysinfo.ps1` | паспорт стенда `SYSTEM.md` (Windows) |
| `check_report.py` | (в шаблоне репозитория студента) проверка комплектности отчёта, запускается в CI |

## Договорённость о выводе программ

Программа ДЗ печатает, кроме обычного вывода, две служебные строки:

```
RESULT 3.1415926536     # контрольное значение: по нему видно, что ответ не «поплыл»
TIME 0.123456           # время расчётной части, измеренное omp_get_wtime()
```

```c
double t0 = omp_get_wtime();
/* ... расчёт ... */
double t = omp_get_wtime() - t0;
printf("RESULT %.10f\nTIME %.6f\n", result, t);
```

Если строки `TIME` нет, `bench.py` измерит время всего процесса (вместе с запуском и инициализацией) и предупредит об этом.

## bench.py

```bash
python3 tools/bench.py --cmd "./build/pi {n}" \
    --threads 1,2,4,8 \
    --set n=10000000,100000000 \
    --env OMP_SCHEDULE="static;dynamic,1;guided" \
    --repeat 5 --out results/pi.csv
```

- `--cmd` — команда; `{имя}` подставляется из `--set`, `{threads}` — текущее число потоков. На Windows (MSYS2): `--cmd "build\pi.exe {n}"`.
- `--threads 1,2,4,8` или диапазон `--threads 1-8`. Переменная `OMP_NUM_THREADS` выставляется автоматически.
- `--set имя=v1,v2` — параметр программы (можно повторять). Значения перечисляются через запятую.
- `--env ИМЯ="a;b;c"` — переменная окружения; значения через **точку с запятой**, потому что в `OMP_SCHEDULE` есть запятые (можно повторять).
- `--repeat 5` — повторов на конфигурацию; `--warmup 1` — прогревочных запусков (в CSV не пишутся).
- `--append` — дописать в существующий CSV (заголовок должен совпасть).
- `--shuffle` — перемешать порядок конфигураций: нагрев ноутбука не будет систематически «наказывать» последние конфигурации.
- `--timeout 600` — предел одного запуска в секундах.

Результат — CSV `threads,<параметры>,<env>,rep,time,result` и рядом `pi.meta.json` (машина, дата, команда, переменные `OMP_*`). Если при одинаковых параметрах программа выдала разные `RESULT`, скрипт предупредит: это повод проверить гонки или порядок суммирования.

## plot.py

```bash
# время по числу потоков
python3 tools/plot.py results/pi.csv --kind time -o plots/pi_time.png
# ускорение для разных schedule при фиксированном n
python3 tools/plot.py results/pi.csv --kind speedup --group OMP_SCHEDULE --filter n=100000000 -o plots/pi_speedup.png
# эффективность
python3 tools/plot.py results/pi.csv --kind efficiency --filter n=100000000 -o plots/pi_eff.png
# таблица медиан для вставки в REPORT.md
python3 tools/plot.py results/pi.csv --kind speedup --group OMP_SCHEDULE --table
```

- Точка — медиана повторов, «усы» — min–max (`--spread iqr` — межквартильный размах, `--spread none` — без разброса).
- Ускорение `S(p) = T(1) / T(p)` по медианам внутри одной кривой; эффективность `E(p) = S(p)/p`. Если точки `threads=1` нет, база — минимальное значение по оси X (на графике будет пометка).
- Столбцы-параметры, которые меняются, но не указаны в `--group`/`--filter`, автоматически дают отдельные кривые (чтобы не смешивать разные `n`).
- `--x n --kind time --logx --logy` — зависимость времени от размера задачи.
- Пунктир «идеальное» — линейное ускорение (отключается `--no-ideal`).

## sysinfo

```bash
bash tools/sysinfo.sh                 # Linux / WSL / macOS → SYSTEM.md
CC=gcc-14 bash tools/sysinfo.sh       # с конкретным компилятором
```

```powershell
powershell -ExecutionPolicy Bypass -File tools\sysinfo.ps1        # Windows
```

Скрипты определяют процессор, физические и логические ядра, P/E-ядра (гибридные Intel и Apple Silicon), кэши, ОЗУ, ОС, компилятор, версию OpenMP по макросу `_OPENMP` и режим электропитания. Строки с пометкой «вручную» заполните сами.

**Замечание о версии OpenMP.** GCC 9–14 сообщает `_OPENMP = 201511` (4.5), хотя поддерживает большую часть 5.0–5.1; clang 18 сообщает 5.1. Для курса достаточно 4.5.
