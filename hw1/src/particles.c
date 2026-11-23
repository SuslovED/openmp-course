/* ДЗ1, часть B. Энергия и силы парного взаимодействия N частиц.
 *
 * Частицы стоят в узлах кубической решётки n x n x n со случайным сдвигом.
 *   нечётный V: потенциал Леннард-Джонса  u(r) = 4 (r^-12 - r^-6)        (eps = sigma = 1)
 *   чётный V:   кулоновский               u(r) = q_i q_j / r,  q = +-1 в шахматном порядке (NaCl)
 * E = sum_{i<j} u(r_ij),  F_i = -grad_i E.  Без обрезания потенциала: все N(N-1)/2 пар.
 *
 * Последовательная версия использует третий закон Ньютона: пару (i,j) считаем один раз,
 * прибавляем силу к F_i и вычитаем из F_j. Это треугольный цикл + запись в чужой F[j].
 *
 * Запуск:
 *   ./particles V info
 *   ./particles V [seq|full|tri] [n]  (по умолчанию tri; n - сторона решётки вместо
 *                                     вариантной, для быстрых тестов и TSan)
 *   ./particles V check-full [n]      сравнить full (или tri) с seq (для make test):
 *   ./particles V check-tri [n]       RESULT <макс. относительное расхождение E и F>
 * Вывод: RESULT <энергия>, FNORM <норма вектора сил>, TIME <секунды>.
 *
 * ЗАДАНИЕ: реализуйте две параллельные версии.
 *   full - «полный» цикл: для каждого i перебираем все j != i и считаем только F_i
 *          (работы вдвое больше, зато каждый поток пишет только свои F_i);
 *   tri  - треугольный цикл j > i, как в seq, без гонок за F[j].
 * Расписание - schedule(runtime).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>

typedef struct { int n_side, lj; double a; } variant_t;

static variant_t variant(int V)
{
    variant_t p;
    p.n_side = 24 + (V - 1) % 6;      /* 24..29 -> N = 13824..24389 */
    p.lj     = V % 2;                 /* 1 - Леннард-Джонс, 0 - кулон */
    p.a      = p.lj ? 1.12 : 1.0;     /* шаг решётки */
    return p;
}

static void make_system(int V, variant_t p, int n, double *x, double *y, double *z, double *q)
{
    unsigned s = 2654435761u * (unsigned)V;
    int k = 0;
    for (int i = 0; i < p.n_side; i++)
        for (int j = 0; j < p.n_side; j++)
            for (int l = 0; l < p.n_side; l++, k++) {
                s = 1664525u * s + 1013904223u; x[k] = p.a * (i + 0.1 * (s / 4294967296.0 - 0.5));
                s = 1664525u * s + 1013904223u; y[k] = p.a * (j + 0.1 * (s / 4294967296.0 - 0.5));
                s = 1664525u * s + 1013904223u; z[k] = p.a * (l + 0.1 * (s / 4294967296.0 - 0.5));
                q[k] = ((i + j + l) % 2) ? -1.0 : 1.0;
            }
    (void)n;
}

/* Энергия пары и множитель силы: F_i += c * (r_i - r_j), F_j -= c * (r_i - r_j) */
static inline double pair(int lj, double qi, double qj, double r2, double *c)
{
    if (lj) {
        double ir2 = 1.0 / r2, ir6 = ir2 * ir2 * ir2;
        *c = 24.0 * ir2 * ir6 * (2.0 * ir6 - 1.0);
        return 4.0 * ir6 * (ir6 - 1.0);
    } else {
        double ir = 1.0 / sqrt(r2), e = qi * qj * ir;
        *c = e * ir * ir;
        return e;
    }
}

/* Эталон - не меняйте */
static double forces_seq(int n, int lj, const double *x, const double *y, const double *z,
                         const double *q, double *fx, double *fy, double *fz)
{
    double e = 0.0;
    for (int i = 0; i < n; i++) fx[i] = fy[i] = fz[i] = 0.0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            double dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j], c;
            e += pair(lj, q[i], q[j], dx * dx + dy * dy + dz * dz, &c);
            fx[i] += c * dx; fy[i] += c * dy; fz[i] += c * dz;
            fx[j] -= c * dx; fy[j] -= c * dy; fz[j] -= c * dz;
        }
    return e;
}

/* Полный цикл: каждый поток пишет только силу своей частицы */
static double forces_full(int n, int lj, const double *x, const double *y, const double *z,
                          const double *q, double *fx, double *fy, double *fz)
{
    double e = 0.0;
    #pragma omp parallel for default(none) shared(n, lj, x, y, z, q, fx, fy, fz) \
        reduction(+: e) schedule(runtime)
    for (int i = 0; i < n; i++) {
        double sx = 0.0, sy = 0.0, sz = 0.0;
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            double dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j], c;
            e += pair(lj, q[i], q[j], dx * dx + dy * dy + dz * dz, &c);
            sx += c * dx;
            sy += c * dy;
            sz += c * dz;
        }
        fx[i] = sx;
        fy[i] = sy;
        fz[i] = sz;
    }
    return e / 2.0;
}

/* Треугольный цикл с приватными копиями массивов сил */
static double forces_tri(int n, int lj, const double *x, const double *y, const double *z,
                         const double *q, double *fx, double *fy, double *fz)
{
    double e = 0.0;
    for (int i = 0; i < n; i++) fx[i] = fy[i] = fz[i] = 0.0;
    #pragma omp parallel for default(none) shared(n, lj, x, y, z, q) \
        reduction(+: e, fx[0:n], fy[0:n], fz[0:n]) schedule(runtime)
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            double dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j], c;
            e += pair(lj, q[i], q[j], dx * dx + dy * dy + dz * dz, &c);
            fx[i] += c * dx; fy[i] += c * dy; fz[i] += c * dz;
            fx[j] -= c * dx; fy[j] -= c * dy; fz[j] -= c * dz;
        }
    return e;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "использование: %s V [seq|full|tri|check-full|check-tri|info] [n]\n", argv[0]); return 1; }
    int V = atoi(argv[1]);
    if (V < 1 || V > 17) { fprintf(stderr, "V должен быть 1..17\n"); return 1; }
    variant_t p = variant(V);
    if (argc > 3) p.n_side = atoi(argv[3]);
    int n = p.n_side * p.n_side * p.n_side;
    const char *mode = (argc > 2) ? argv[2] : "tri";
    if (strcmp(mode, "info") == 0) {
        printf("V=%d: N=%d (%d^3), потенциал %s, шаг решётки %.2f\n", V, n, p.n_side,
               p.lj ? "Леннард-Джонс" : "кулоновский", p.a);
        return 0;
    }

    double *x = malloc(n * sizeof *x), *y = malloc(n * sizeof *y), *z = malloc(n * sizeof *z);
    double *q = malloc(n * sizeof *q);
    double *fx = malloc(n * sizeof *fx), *fy = malloc(n * sizeof *fy), *fz = malloc(n * sizeof *fz);
    make_system(V, p, n, x, y, z, q);

    if (strncmp(mode, "check-", 6) == 0) {
        double *gx = malloc(n * sizeof *gx), *gy = malloc(n * sizeof *gy), *gz = malloc(n * sizeof *gz);
        double e1 = forces_seq(n, p.lj, x, y, z, q, fx, fy, fz), e2;
        if (strcmp(mode + 6, "full") == 0) e2 = forces_full(n, p.lj, x, y, z, q, gx, gy, gz);
        else                               e2 = forces_tri (n, p.lj, x, y, z, q, gx, gy, gz);
        double fmaxv = 0.0, dmax = 0.0;
        for (int i = 0; i < n; i++) {
            fmaxv = fmax(fmaxv, fmax(fabs(fx[i]), fmax(fabs(fy[i]), fabs(fz[i]))));
            dmax  = fmax(dmax, fmax(fabs(fx[i] - gx[i]), fmax(fabs(fy[i] - gy[i]), fabs(fz[i] - gz[i]))));
        }
        double rel = fmax(fabs(e1 - e2) / fabs(e1), dmax / fmaxv);
        printf("check %s: отн. расхождение E %.2e, F %.2e\nRESULT %.3e\n", mode + 6,
               fabs(e1 - e2) / fabs(e1), dmax / fmaxv, rel);
        free(gx); free(gy); free(gz);
        return 0;
    }

    double t0 = omp_get_wtime(), e;
    if      (strcmp(mode, "seq")  == 0) e = forces_seq (n, p.lj, x, y, z, q, fx, fy, fz);
    else if (strcmp(mode, "full") == 0) e = forces_full(n, p.lj, x, y, z, q, fx, fy, fz);
    else if (strcmp(mode, "tri")  == 0) e = forces_tri (n, p.lj, x, y, z, q, fx, fy, fz);
    else { fprintf(stderr, "режим: seq | full | tri\n"); return 1; }
    double t = omp_get_wtime() - t0;

    double fn = 0.0;
    for (int i = 0; i < n; i++) fn += fx[i] * fx[i] + fy[i] * fy[i] + fz[i] * fz[i];
    printf("V=%d N=%d %s, потоков %d, E/N = %.10f\n", V, n, mode, omp_get_max_threads(), e / n);
    printf("RESULT %.15e\nFNORM %.15e\nTIME %.6f\n", e, sqrt(fn), t);
    free(x); free(y); free(z); free(q); free(fx); free(fy); free(fz);
    return 0;
}
