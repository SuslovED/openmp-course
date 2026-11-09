/* С1. Области видимости: shared, private, firstprivate.
 * Предскажите вывод до запуска: OMP_NUM_THREADS=4 ./scope
 */
#include <stdio.h>
#include <omp.h>

int main(void)
{
    int a = 10, b = 20, c = 30;

    #pragma omp parallel private(b) firstprivate(c) shared(a) num_threads(4)
    {
        int id = omp_get_thread_num();
        b = id;            /* private: значение до региона НЕ копируется, его надо присвоить */
        c += id;           /* firstprivate: каждая копия начинается с 30 */
        if (id == 0) a = 99;   /* shared: изменение видят все (после барьера) */
        #pragma omp barrier
        #pragma omp critical
        printf("поток %d: a=%d b=%d c=%d\n", id, a, b, c);
    }
    /* После региона: a изменилась, b и c - исходные значения (копии уничтожены) */
    printf("после: a=%d b=%d c=%d\n", a, b, c);
    return 0;
}
