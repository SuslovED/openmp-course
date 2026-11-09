/* С1. Последовательное вычисление pi = int_0^1 4/(1+x^2) dx (формула средних прямоугольников).
 * Запуск: ./pi_serial [N]
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
    for (long i = 0; i < n; i++) {
        double x = (i + 0.5) * h;
        sum += 4.0 / (1.0 + x * x);
    }
    double pi = sum * h;
    double t = omp_get_wtime() - t0;

    printf("pi = %.15f, ошибка = %.2e\n", pi, fabs(pi - PI_REF));
    printf("RESULT %.15f\nTIME %.6f\n", pi, t);
    return 0;
}
