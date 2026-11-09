/* hello.c — проверка, что OpenMP работает.
 * Сборка: make            Запуск: OMP_NUM_THREADS=4 ./build/hello
 */
#include <stdio.h>
#include <omp.h>

int main(void)
{
#ifdef _OPENMP
    printf("Компилятор поддерживает OpenMP, _OPENMP = %d\n", _OPENMP);
#else
    printf("OpenMP НЕ включён: проверьте флаг -fopenmp\n");
#endif
    printf("Процессоров доступно: %d, потоков по умолчанию: %d\n",
           omp_get_num_procs(), omp_get_max_threads());

#pragma omp parallel
    {
        int id = omp_get_thread_num();
        int nt = omp_get_num_threads();
        printf("Привет от потока %d из %d\n", id, nt);
    }
    return 0;
}
