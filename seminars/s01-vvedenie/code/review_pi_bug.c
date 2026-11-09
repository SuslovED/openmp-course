/* С1. Разбор чужого кода. Решение с форума на вопрос «как распараллелить вычисление pi
 * с OpenMP и измерить ускорение». Пояснение автора (почти дословно):
 *   "Вот эффективная параллельная версия: каждый поток накапливает частичную сумму
 *    в своём элементе массива, что исключает гонки данных. Время измеряем через clock()."
 *
 * Запустите на 1, 2, 4 потоках. Ответ правильный. Значит, программа правильная?
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 200000000L;
    double step = 1.0 / (double)n;
    int nthreads = omp_get_num_threads();      /* количество потоков */
    double partial[nthreads];                  /* частичные суммы для каждого потока */
    for (int k = 0; k < nthreads; k++) partial[k] = 0.0;

    clock_t start = clock();
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        for (long i = id; i < n; i += nthreads) {
            double x = (i + 0.5) * step;
            partial[id] += 4.0 / (1.0 + x * x);
        }
    }
    double sum = 0.0;
    for (int k = 0; k < nthreads; k++) sum += partial[k];
    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;

    printf("pi = %.12f\n", sum * step);
    printf("Время: %.3f с\n", elapsed);
    printf("RESULT %.15f\nTIME %.6f\n", sum * step, elapsed);
    return 0;
}
