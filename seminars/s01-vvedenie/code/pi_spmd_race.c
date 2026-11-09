/* С1. SPMD-разбиение «как в MPI»: каждый поток берёт свой кусок индексов.
 * ОШИБКА (намеренная): все потоки пишут в общую переменную sum -> гонка данных.
 * До запуска запишите прогноз: что получится на 2 и 4 потоках? Сравните gcc и clang.
 * Проверка: clang -fopenmp -fsanitize=thread -g pi_spmd_race.c && OMP_NUM_THREADS=2 ./a.out 1000000
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
        int id = omp_get_thread_num();     /* аналог MPI rank */
        int nt = omp_get_num_threads();    /* аналог MPI size */
        for (long i = id; i < n; i += nt) {  /* циклическое разбиение */
            double x = (i + 0.5) * h;
            sum += 4.0 / (1.0 + x * x);     /* ГОНКА: read-modify-write общей sum */
        }
    }
    double pi = sum * h;
    double t = omp_get_wtime() - t0;

    printf("pi = %.15f, ошибка = %.2e\n", pi, fabs(pi - PI_REF));
    printf("RESULT %.15f\nTIME %.6f\n", pi, t);
    return 0;
}
