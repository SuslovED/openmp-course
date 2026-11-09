/* С1. Частичные суммы в массиве partial[id] (как MPI_Gather + сложение на корне).
 * Гонки нет, но соседние элементы partial[] лежат в одной кэш-линии (64 байта):
 * потоки «перетягивают» её друг у друга -> false sharing (подробно в С4).
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define PI_REF 3.14159265358979323846

#define MAXT 256

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 400000000L;
    double h = 1.0 / (double)n;
    double partial[MAXT] = {0.0};
    int nthreads = 1;

    double t0 = omp_get_wtime();
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        int nt = omp_get_num_threads();
        if (id == 0) nthreads = nt;      /* узнать число потоков можно только внутри региона */
        for (long i = id; i < n; i += nt) {
            double x = (i + 0.5) * h;
            partial[id] += 4.0 / (1.0 + x * x);
        }
    }
    double sum = 0.0;
    for (int k = 0; k < nthreads; k++) sum += partial[k];
    double pi = sum * h;
    double t = omp_get_wtime() - t0;

    printf("потоков %d, pi = %.15f, ошибка = %.2e\n", nthreads, pi, fabs(pi - PI_REF));
    printf("RESULT %.15f\nTIME %.6f\n", pi, t);
    return 0;
}
