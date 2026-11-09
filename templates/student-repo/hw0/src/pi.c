/* pi.c — вычисление pi = интеграл от 0 до 1 функции 4/(1+x^2) методом средних прямоугольников.
 * Параллельная версия «как на С1»: каждый поток считает свою частичную сумму,
 * затем добавляет её к общей через atomic.
 *
 * Сборка: make           Запуск: OMP_NUM_THREADS=4 ./build/pi 100000000
 * Вывод: RESULT <значение>, TIME <секунды> — их разбирает tools/bench.py.
 */
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 100000000L;
    if (n <= 0) {
        fprintf(stderr, "использование: %s [n > 0]\n", argv[0]);
        return 1;
    }
    const double h = 1.0 / (double)n;
    double sum = 0.0;

    double t0 = omp_get_wtime();
#pragma omp parallel default(none) shared(n, h, sum)
    {
        int id = omp_get_thread_num();
        int nt = omp_get_num_threads();
        double local = 0.0;
        /* циклическое распределение итераций по потокам */
        for (long i = id; i < n; i += nt) {
            double x = (i + 0.5) * h;
            local += 4.0 / (1.0 + x * x);
        }
#pragma omp atomic
        sum += local;
    }
    double pi = sum * h;
    double t = omp_get_wtime() - t0;

    printf("n = %ld, потоков = %d, pi = %.12f\n", n, omp_get_max_threads(), pi);
    printf("RESULT %.12f\n", pi);
    printf("TIME %.6f\n", t);
    return 0;
}
