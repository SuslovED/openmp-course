# Окружение для курса: установка OpenMP и методика замеров на ноутбуке

Эта инструкция для студентов. Цель — к С2 у каждого на ноутбуке собирается и запускается программа с OpenMP, работают `make test` и `tools/bench.py`, а в репозитории лежит паспорт стенда `SYSTEM.md` (это ДЗ0).

**Что понадобится в итоге:**

| Компонент | Зачем |
|---|---|
| компилятор C с OpenMP ≥ 4.5 (gcc ≥ 12 или clang ≥ 14 + libomp) | сборка программ |
| `make` | сборка по Makefile из шаблона |
| clang + libomp + рантайм TSan (желательно) | поиск гонок `make tsan`; если не получилось — гонки ищет CI |
| Python 3.8+ и matplotlib | замеры `bench.py` и графики `plot.py` |
| git + аккаунт GitHub (или GitLab) | сдача ДЗ, CI |
| редактор (рекомендуем VS Code) | |

Короткая рекомендация: **Linux — ставьте пакеты; Windows — WSL2 + Ubuntu; macOS — Homebrew + gcc (или libomp).**

---

## 1. Linux (Ubuntu / Debian; для других дистрибутивов пакеты называются похоже)

```bash
sudo apt update
sudo apt install -y build-essential git python3 python3-matplotlib \
                    clang libomp-dev
# рантайм ThreadSanitizer для clang (номер версии = версия clang, см. clang --version)
sudo apt install -y libclang-rt-18-dev        # Ubuntu 24.04; на 22.04 он входит в пакет clang
```

Проверка:

```bash
gcc --version                 # gcc ≥ 12 (нужна директива masked)
echo '#include <omp.h>
#include <stdio.h>
int main(void){
#pragma omp parallel
printf("поток %d из %d\n", omp_get_thread_num(), omp_get_num_threads());
printf("_OPENMP = %d\n", _OPENMP); }' > t.c
gcc -fopenmp t.c -o t && OMP_NUM_THREADS=4 ./t
clang -fopenmp t.c -o t && ./t                      # проверка clang + libomp
```

Fedora: `sudo dnf install gcc make clang libomp-devel compiler-rt python3-matplotlib git`.
Arch: `sudo pacman -S base-devel clang openmp compiler-rt python-matplotlib git`.

---

## 2. Windows

### Вариант А (рекомендуемый): WSL2 + Ubuntu

WSL2 — настоящий Linux внутри Windows. В нём работают gcc, clang, TSan, те же команды, что в CI и на слайдах. Производительность вычислений такая же, как в «родной» Windows.

1. Откройте **PowerShell от имени администратора** и выполните:
   ```powershell
   wsl --install -d Ubuntu-24.04
   ```
   Перезагрузите компьютер, когда попросит. При первом запуске Ubuntu придумайте имя пользователя и пароль.
2. В терминале Ubuntu выполните команды из раздела 1 (Linux). Если `sudo apt update` пишет `Could not connect to archive.ubuntu.com` или `connection timed out`, а потом `apt install` не находит пакеты, у WSL нет доступа в сеть — решение в разделе 7, первая строка таблицы.
3. **Храните репозиторий внутри WSL** (`~/openmp-курс/…`), а не в `/mnt/c/…`: доступ к файлам Windows из WSL в разы медленнее, сборка и `git` будут тормозить.
4. VS Code: установите на Windows VS Code и расширение **WSL** (Microsoft). В терминале Ubuntu, в каталоге репозитория, наберите `code .`, и редактор откроется «внутри» WSL.
5. По умолчанию WSL видит все логические процессоры. Проверьте: `nproc`. Если меньше, чем в Windows, посмотрите файл `%UserProfile%\.wslconfig` (параметр `processors=`).

### Вариант Б: MSYS2 (UCRT64), если WSL недоступен

1. Установите MSYS2 с https://www.msys2.org/ и откройте ярлык **MSYS2 UCRT64** (не MSYS и не MINGW32).
2. ```bash
   pacman -Syu          # может попросить перезапустить терминал — перезапустите и повторите
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc make git \
         mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-python-matplotlib
   ```
3. Проверка — та же программа `t.c`: `gcc -fopenmp t.c -o t.exe && OMP_NUM_THREADS=4 ./t.exe`.
4. Ограничение: **ThreadSanitizer под MinGW нет**. Цель `make tsan` в MSYS2 сообщит об этом. Гонки будет искать CI в репозитории, или запускайте `make tsan` в WSL.

### Почему не Visual Studio (MSVC)

Ключ `/openmp` в MSVC включает **OpenMP 2.0 (2002 год)**. В нём нет `collapse`, задач (`task`), редукций `min`/`max`, `simd`, беззнаковых счётчиков циклов, а `reduction` по массивам не поддерживается. Ключ `/openmp:llvm` добавляет часть возможностей более новых версий, но не полный OpenMP 4.5. Примеры курса и CI рассчитаны на gcc/clang, и код «под MSVC» придётся переписывать. Готовые примеры из интернета, кстати, под MSVC часто не компилируются, а «версии под MSVC» оказываются урезанными. Если очень хочется Visual Studio, ставьте в нём компонент **Clang для Windows** и используйте clang с `-fopenmp` (уточните у преподавателя).

---

## 3. macOS

Стандартный компилятор macOS (Apple clang, команда `clang`, а также `gcc`, который на самом деле тоже Apple clang) **не понимает `-fopenmp`** и не содержит рантайма OpenMP. Есть два пути. Оба требуют Homebrew (https://brew.sh) и Command Line Tools (`xcode-select --install`).

### Вариант А (рекомендуемый): настоящий gcc из Homebrew

```bash
brew install gcc python-matplotlib
gcc-15 --version          # номер версии смотрите в выводе brew (gcc-14, gcc-15, …)
gcc-15 -fopenmp t.c -o t && OMP_NUM_THREADS=4 ./t
```

Makefile из шаблона сам находит `gcc-NN`. Вручную: `make CC=gcc-15`.

### Вариант Б: Apple clang + libomp

```bash
brew install libomp
clang -Xpreprocessor -fopenmp -I"$(brew --prefix libomp)/include" \
      -L"$(brew --prefix libomp)/lib" -lomp t.c -o t
```

`libomp` в Homebrew «keg-only», поэтому пути нужно указывать явно. Шаблон подставляет их сам, если `gcc-NN` не найден.

### ThreadSanitizer на macOS

- Лучше всего поставить `brew install llvm`: шаблон возьмёт `$(brew --prefix llvm)/bin/clang` для `make tsan`.
- Без него используется Apple clang + libomp. TSan работает, но без модуля Archer возможны ложные срабатывания внутри рантайма OpenMP. Окончательный вердикт выносит CI на Linux.

### Особенности Apple Silicon (M1–M4)

- Ядра двух типов: производительные (P) и энергоэффективные (E). Например, M2: 4P + 4E. Ускорение на 8 потоках не будет «8»: E-ядра заметно медленнее.
- macOS **не позволяет привязать поток к ядру**: `OMP_PROC_BIND` и `OMP_PLACES` фактически не действуют. Это нормально, просто отметьте в отчёте.
- SMT (гиперпоточности) нет: физических ядер = логических.

---

## 4. Проверка версии OpenMP

```c
printf("_OPENMP = %d\n", _OPENMP);
```

| `_OPENMP` | Версия |
|---|---|
| 200505 | 2.5 |
| 200805 | 3.0 |
| 201107 | 3.1 |
| 201307 | 4.0 |
| 201511 | 4.5 |
| 201811 | 5.0 |
| 202011 | 5.1 |
| 202111 | 5.2 |
| 202411 | 6.0 |

GCC 9–14 сообщает 201511 (4.5), хотя реализует большую часть 5.0–5.1. clang 18 сообщает 202011 (5.1). Для курса достаточно 4.5. Отдельные места, где нужна более новая версия, оговорены на слайдах.

---

## 5. Git, репозиторий и CI

1. Установите git (Linux/WSL: уже установлен выше; macOS: входит в Command Line Tools; MSYS2: `pacman -S git`), настройте:
   ```bash
   git config --global user.name  "Фамилия Имя"
   git config --global user.email "почта@пример.ру"
   ```
2. Заведите аккаунт на GitHub (или GitLab, если так договорились на курсе), настройте вход по SSH-ключу или токену.
3. Создайте **приватный** репозиторий из шаблона курса (`templates/student-repo/`, кнопка *Use this template* или ссылка от преподавателя), добавьте преподавателя в соавторы.
4. Склонируйте его, перейдите в `hw0/`, выполните `make info && make && make test && make tsan`.
5. После `git push` откройте вкладку **Actions**: должны пройти все задачи: `build + test (gcc)`, `build + test (clang)`, `ThreadSanitizer`, `Комплектность отчётов`.

**VS Code:** расширения *C/C++* (Microsoft), *Makefile Tools* (по желанию), *WSL* (для Windows), *Python*. Расширения для слайдов (Marp) студентам не нужны. Чтобы IntelliSense не подчёркивал `#pragma omp` и `omp.h`, в `.vscode/c_cpp_properties.json` добавьте `"compilerArgs": ["-fopenmp"]` или укажите `compilerPath` на ваш gcc.

---

## 6. Методика замеров на ноутбуке

Ноутбук — капризный измерительный прибор: частота процессора зависит от температуры, питания и фоновой нагрузки. Без дисциплины разброс времени между запусками легко достигает 20–30%, и выводы об ускорении превращаются в шум.

### Перед замерами
1. **Питание от сети**, режим электропитания «Максимальная производительность»:
   Windows — *Параметры → Система → Питание → Режим питания*; macOS — отключить *Режим энергосбережения*; Linux — `powerprofilesctl set performance` (если есть).
2. **Закройте браузер, мессенджеры, IDE с индексацией**, торренты, облачную синхронизацию. В WSL всё равно закройте программы Windows: ядра общие.
3. Ноутбук на твёрдой поверхности, не на коленях и не на одеяле (термотроттлинг).
4. Не запускайте замеры сразу после сборки большого проекта: дайте процессору остыть 1–2 минуты.

### Во время замеров
5. **Прогрев:** первый запуск обычно медленнее (холодные кэши, подгрузка страниц памяти). `bench.py` по умолчанию делает 1 прогревочный запуск.
6. **Не меньше 5 повторов**, в отчёт идёт **медиана**, разброс показывается «усами» (min–max). Если разброс больше 10%, ищите причину и повторяйте.
7. Время расчётной части меряется `omp_get_wtime()`, а не `clock()`: `clock()` суммирует процессорное время всех потоков и «показывает замедление» при росте их числа.
8. Размер задачи подбирайте так, чтобы на 1 потоке расчёт шёл **0,5–5 секунд**. Меньше — шум и накладные расходы, больше — нагрев и долгое ожидание.
9. При длинных сериях используйте `--shuffle`, чтобы нагрев не «наказывал» систематически последние конфигурации.

### Ядра, потоки, привязка
10. **Физические и логические ядра.** У процессора с SMT (Hyper-Threading) логических процессоров вдвое больше, чем физических ядер. Для вычислительных задач ускорение выше числа физических ядер редко. Мерьте `--threads 1,2,…,<физ. ядер>,<логич. ядер>`, чтобы увидеть это своими глазами.
11. **Гибридные процессоры (Intel 12-го поколения и новее, Apple Silicon, часть AMD).** P-ядра и E-ядра работают с разной скоростью. При `schedule(static)` поток на E-ядре отстаёт, и все ждут его на барьере. Это реальный эффект, его стоит обсуждать в отчёте, а не прятать.
12. **Привязка потоков** (Linux, WSL, Windows через libgomp/libomp):
    ```bash
    export OMP_PLACES=cores        # одно место = одно физическое ядро
    export OMP_PROC_BIND=close     # или spread
    export OMP_DISPLAY_ENV=true    # рантайм напечатает, что он понял
    ```
    Подробно разбирается на С4. В отчёте указывайте, были ли эти переменные заданы.
13. **Не запускайте больше потоков, чем логических процессоров**, если не исследуете специально переподписку.

### Что записать в SYSTEM.md и отчёт
- модель процессора, число физических/логических ядер, P/E, кэши, ОЗУ, ОС, компилятор и флаги (генерирует `tools/sysinfo.*`);
- питание (сеть/батарея) и режим электропитания;
- переменные `OMP_*`, если меняли;
- в ДЗ3 — измеренную полосу памяти (STREAM).

### Если у вас 2 ядра (или ноутбук совсем слабый)
На 2 ядрах половина экспериментов курса неинформативна. Напишите преподавателю **до** сдачи ДЗ1: договоримся о машине в компьютерном классе или об учётной записи на кластере (тот же, что использовался в части про MPI). Тогда в `SYSTEM.md` описывается эта машина, а программы и замеры запускаются там.

---

## 7. Частые проблемы и решения

| Симптом | Причина и решение |
|---|---|
| WSL: `sudo apt update` пишет `Could not connect to archive.ubuntu.com:80 … connection timed out`, затем `Unable to locate package` | У WSL нет доступа к серверам Ubuntu. Проверьте: `curl -sS -m 10 -o /dev/null -w '%{http_code}\n' https://mirrors.edge.kernel.org/ubuntu/`. **Нет ответа** — в WSL нет интернета, чаще всего из-за VPN в Windows: выключите VPN на время установки или (Windows 11) добавьте в `%UserProfile%\.wslconfig` в секцию `[wsl2]` строки `networkingMode=mirrored` и `dnsTunneling=true`, затем в PowerShell `wsl --shutdown`. **Ответ 200** — интернет есть, недоступен только archive.ubuntu.com: `sudo sed -i 's#http://[a-z]*.ubuntu.com/ubuntu/#https://mirrors.edge.kernel.org/ubuntu/#' /etc/apt/sources.list.d/ubuntu.sources`, затем снова `sudo apt update`. |
| `fatal error: omp.h: No such file or directory` (gcc) | Компилируете без `-fopenmp` или установлен не тот gcc. Проверьте `make info`. На macOS `gcc` — это Apple clang, используйте `gcc-15`. |
| `'omp.h' file not found` (clang) | Не установлен libomp: `sudo apt install libomp-dev`; macOS — `brew install libomp` и флаги `-Xpreprocessor -fopenmp -I…`. |
| `clang: error: unsupported option '-fopenmp'` (macOS) | Это Apple clang. См. раздел 3: gcc из Homebrew или `-Xpreprocessor -fopenmp` + libomp. |
| `undefined reference to omp_get_wtime` / `omp_get_thread_num` | Флаг `-fopenmp` не передан на этапе **компоновки**. В шаблоне это сделано, а при ручной сборке ставьте флаг и при компиляции, и при линковке. |
| Программа работает, но всегда 1 поток | Не включён OpenMP при сборке (директивы проигнорированы, ищите предупреждение `-Wunknown-pragmas`), или `OMP_NUM_THREADS=1` в окружении (`echo $OMP_NUM_THREADS`). |
| `make: command not found` (MSYS2) | `pacman -S make`; работайте в терминале **UCRT64**. |
| `make tsan`: `cannot find …libclang_rt.tsan…a` | Нет рантайма TSan: Ubuntu 24.04 — `sudo apt install libclang-rt-18-dev` (номер = версия clang). |
| TSan: `FATAL: ThreadSanitizer: unexpected memory mapping` | Новые ядра Linux используют большую случайность адресов: `sudo sysctl -w vm.mmap_rnd_bits=28` (до перезагрузки). |
| TSan сообщает о гонке внутри `libomp.so` / `libgomp.so`, а не в вашем коде | Ложное срабатывание: собирайте clang-ом с libomp (не gcc), шаблон подключает модуль Archer. `make info` должна показывать найденный `ARCHER_LIB`. |
| В WSL сборка и `git status` очень медленные | Репозиторий лежит в `/mnt/c/…`. Перенесите его в домашний каталог WSL (`~`). |
| `bench.py`: «программа не печатает строку TIME» | Добавьте в программу `printf("TIME %.6f\n", t);`, иначе в CSV попадёт время всего процесса с запуском. |
| `ModuleNotFoundError: No module named 'matplotlib'` | `sudo apt install python3-matplotlib` / `pip install --user matplotlib` / в MSYS2 пакет `mingw-w64-ucrt-x86_64-python-matplotlib`. |
| `powershell`: «выполнение сценариев отключено» | Запускайте так: `powershell -ExecutionPolicy Bypass -File tools\sysinfo.ps1`. |
| Скрипты `.sh` из репозитория на Windows падают с `$'\r': command not found` | Файлы получили переводы строк Windows. В шаблоне есть `.gitattributes`; выполните `git rm --cached -r . && git reset --hard` или клонируйте репозиторий заново внутри WSL. |
| Время скачет на ±30% между запусками | Батарея, фоновые программы, нагрев: см. раздел 6. Увеличьте `--repeat`, используйте медиану. |
| Ускорение > числа ядер («сверхлинейное») | Иногда реально: данные одного потока перестали помещаться в кэш, а у p потоков поместились. Но сначала проверьте, что T(1) мерили той же программой с `OMP_NUM_THREADS=1`, а не другой версией. |
| CI красный в `check-report`, хотя всё сделано | Не удалена строка `template:unfilled` в `REPORT.md`/`SOURCES.md`, или не закоммичены `results/*.csv` и `plots/*.png`. Проверьте локально: `python3 tools/check_report.py hwN`. |
