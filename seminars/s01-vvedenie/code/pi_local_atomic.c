/* С1. Правильный шаблон: локальная сумма в каждом потоке + одно атомарное сложение в конце.
 * Ровно это делает reduction(+:sum) (С2).
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
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        int nt = omp_get_num_threads();
        double local = 0.0;                 /* private: объявлена внутри региона */
        for (long i = id; i < n; i += nt) {
            double x = (i + 0.5) * h;
            local += 4.0 / (1.0 + x * x);
        }
        #pragma omp atomic
        sum += local;                       /* nt атомарных операций вместо n */
    }
    double pi = sum * h;
    double t = omp_get_wtime() - t0;

    printf("pi = %.15f, ошибка = %.2e\n", pi, fabs(pi - PI_REF));
    printf("RESULT %.15f\nTIME %.6f\n", pi, t);
    return 0;
}
