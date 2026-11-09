/* С1. Почему нельзя мерить время через clock().
 * clock() возвращает процессорное время ВСЕХ потоков процесса (на Linux/macOS),
 * поэтому при росте числа потоков «время» растёт, хотя программа ускоряется.
 * (На Windows/MSVC clock() возвращает настенное время - ещё одна несовместимость.)
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 400000000L;
    double h = 1.0 / (double)n, sum = 0.0;

    clock_t c0 = clock();
    double  w0 = omp_get_wtime();
    #pragma omp parallel
    {
        int id = omp_get_thread_num(), nt = omp_get_num_threads();
        double local = 0.0;
        for (long i = id; i < n; i += nt) {
            double x = (i + 0.5) * h;
            local += 4.0 / (1.0 + x * x);
        }
        #pragma omp atomic
        sum += local;
    }
    double wall = omp_get_wtime() - w0;
    double cpu  = (double)(clock() - c0) / CLOCKS_PER_SEC;

    printf("pi = %.12f\n", sum * h);
    printf("omp_get_wtime: %.3f с (настенное время)\n", wall);
    printf("clock():       %.3f с (суммарное CPU-время потоков)\n", cpu);
    printf("RESULT %.15f\nTIME %.6f\n", sum * h, wall);
    return 0;
}
