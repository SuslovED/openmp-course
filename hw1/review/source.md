# Источник кода

[OpenmpSem, hw1/src/newton.c](https://github.com/geloidd/OpenmpSem/blob/0bda723a37cf7a8de1a471d923b1621c65446e98/hw1/src/newton.c) — решение пользователя GitHub `geloidd`, вариант 4. Репозиторий скачан командой `git clone --depth 1 https://github.com/geloidd/OpenmpSem.git`. Полный исходник из коммита `0bda723a37cf7a8de1a471d923b1621c65446e98` сохранён в `external.c` побайтно.

В `../src/newton_ext.c` перенесена функция `basins_par` с `collapse(2)`. Сигнатура совпала. В копии убраны таймеры и вывод отдельных потоков в `stderr`, чтобы сравнивать вычисления без затрат на вывод. Остальная программа взята из основной версии ДЗ1, поэтому обе версии используют один и тот же вариант, последовательный эталон и формат `TIME`.
