/* С1. Упражнение после занятия (необязательно): задания 1 и 2 из листка seminars/s01-vvedenie/praktika.md.
 *
 * Задание 1. Распараллельте вычисление pi в стиле SPMD с БЛОЧНЫМ разбиением:
 *   поток id обрабатывает непрерывный диапазон [begin, end), как ранг в MPI.
 *   Аккуратно обработайте остаток, когда n не делится на число потоков.
 *   Частичные суммы соберите без гонки: локальная переменная + critical
 *   (или atomic) ОДИН раз на поток.
 *   Проверка: OMP_NUM_THREADS=3 ./practice/pi_practice 100000007 — ошибка порядка 1e-13.
 * Задание 2. Сначала запишите прогноз ускорения на 2, 4 и 8 потоках, потом заполните
 *   таблицу T(p) для p = 1, 2, 4, ..., 2*(число логических процессоров).
 *
 * Сборка (из каталога code):  make practice/pi_practice
 *   или вручную:              gcc -O2 -fopenmp practice/pi_practice.c -o practice/pi_practice -lm
 * Запуск:  OMP_NUM_THREADS=4 ./practice/pi_practice 400000000
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define PI_REF 3.14159265358979323846

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 400000000L;
    double h = 1.0 / (double)n, sum = 0.0;

    double t0 = omp_get_wtime();

    /* TODO: параллельный регион */
    {
        /* TODO: id, nt, границы своего блока begin/end */
        long begin = 0, end = n;
        double local = 0.0;
        for (long i = begin; i < end; i++) {
            double x = (i + 0.5) * h;
            local += 4.0 / (1.0 + x * x);
        }
        /* TODO: безопасно добавить local к sum */
        sum += local;
    }

    double pi = sum * h;
    double t = omp_get_wtime() - t0;
    printf("pi = %.15f, ошибка = %.2e\n", pi, fabs(pi - PI_REF));
    printf("RESULT %.15f\nTIME %.6f\n", pi, t);
    return 0;
}
