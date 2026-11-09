/* С1. Упражнение после занятия (задание 3 из praktika.md), необязательно.
 * Вычислите I = int_0^3 exp(-x^2) dx формулой Симпсона с n (чётным) отрезками
 * и распараллельте так же, как pi. Точное значение: sqrt(pi)/2 * erf(3).
 * Вопрос: почему ответ на 1 и 4 потоках может отличаться в последних знаках?
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

static double f(double x) { return exp(-x * x); }

int main(int argc, char **argv)
{
    long n = (argc > 1) ? atol(argv[1]) : 200000000L;   /* чётное */
    double a = 0.0, b = 3.0, h = (b - a) / (double)n;
    double s = f(a) + f(b);

    double t0 = omp_get_wtime();
    /* TODO: распараллелить */
    for (long i = 1; i < n; i++)
        s += f(a + i * h) * ((i % 2) ? 4.0 : 2.0);
    double I = s * h / 3.0;
    double t = omp_get_wtime() - t0;

    double exact = 0.5 * sqrt(3.14159265358979323846) * erf(3.0);
    printf("I = %.15f, ошибка = %.2e\n", I, fabs(I - exact));
    printf("RESULT %.15f\nTIME %.6f\n", I, t);
    return 0;
}
