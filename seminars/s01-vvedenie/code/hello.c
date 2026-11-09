/* С1. Первая программа: fork-join.
 * Сборка:  gcc -fopenmp hello.c -o hello
 * Запуск:  OMP_NUM_THREADS=4 ./hello
 */
#include <stdio.h>
#include <omp.h>

int main(void)
{
    printf("До региона: потоков %d\n", omp_get_num_threads());

    #pragma omp parallel
    {
        int id = omp_get_thread_num();      /* своя копия у каждого потока */
        int nt = omp_get_num_threads();
        printf("Привет от потока %d из %d\n", id, nt);
    }   /* неявный барьер: главный поток ждёт всех */

    printf("После региона: потоков %d\n", omp_get_num_threads());
#ifdef _OPENMP
    printf("_OPENMP = %d, доступно процессоров: %d\n", _OPENMP, omp_get_num_procs());
#endif
    return 0;
}
