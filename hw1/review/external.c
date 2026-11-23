/* ДЗ1, часть A. Бассейны притяжения метода Ньютона для p(z) = z^m - 1.
 *
 * Для каждой точки z0 сетки NX x NY в квадрате [cx-w, cx+w] x [cy-w, cy+w]
 * запускаем итерации Ньютона z <- z - p(z)/p'(z) до сходимости и запоминаем,
 * к какому из m корней (exp(2*pi*i*k/m)) пришли и за сколько итераций.
 * Число итераций сильно зависит от точки: около границ бассейнов их много.
 *
 * Запуск:
 *   ./newton V info               параметры варианта
 *   ./newton V NX NY [seq|par]    расчёт (по умолчанию par)
 *   ./newton V NX NY par img      + картинка basins.bmp
 *   ./newton V NX NY check        сравнить par с seq поточечно (для make test):
 *                                 RESULT <число несовпавших точек>, должно быть 0
 * Вывод: RESULT <сумма итераций по всем точкам>, TIME <секунды>.
 * RESULT - целое число, не зависит от числа потоков и расписания.
 *
 * ЗАДАНИЕ: реализуйте basins_par() с OpenMP. Расписание не фиксируйте в коде:
 * используйте schedule(runtime), чтобы перебирать его через OMP_SCHEDULE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>

#define MAXIT 200
#define TOL   1e-10
#define TWO_PI 6.28318530717958647692

typedef struct { int m; double cx, cy, w; } variant_t;

static variant_t variant(int V)
{
    variant_t p;
    p.m  = 3 + (V - 1) % 5;                    /* 3..7 */
    p.cx = -0.3 + 0.3 * ((V - 1) % 3);         /* -0.3, 0, 0.3 */
    p.cy =  0.5 + 0.25 * ((V - 1) % 4);        /* 0.5 .. 1.25: окно сдвинуто вверх */
    p.w  =  1.0 + 0.25 * ((V - 1) % 2);        /* 1.0 или 1.25 */
    return p;
}

/* Итерации Ньютона из точки (x, y). Возвращает число итераций, в *root - номер корня или -1. */
static int newton_point(double x, double y, int m, int *root)
{
    for (int it = 0; it < MAXIT; it++) {
        /* q = z^(m-1) */
        double qr = 1.0, qi = 0.0;
        for (int k = 0; k < m - 1; k++) {
            double t = qr * x - qi * y;
            qi = qr * y + qi * x;
            qr = t;
        }
        double pr = qr * x - qi * y - 1.0, pi = qr * y + qi * x;   /* p = z^m - 1 */
        double dr = m * qr, di = m * qi;                           /* p' = m z^(m-1) */
        double den = dr * dr + di * di;
        if (den < 1e-300) { *root = -1; return it; }               /* попали в 0 */
        double sr = (pr * dr + pi * di) / den, si = (pi * dr - pr * di) / den;
        x -= sr; y -= si;
        if (sr * sr + si * si < TOL * TOL) {
            int k = (int)lround(atan2(y, x) * m / TWO_PI);
            *root = ((k % m) + m) % m;
            return it + 1;
        }
    }
    *root = -1;
    return MAXIT;
}

/* Последовательная версия - эталон, не меняйте */
static long basins_seq(variant_t p, int nx, int ny, signed char *root, unsigned short *iters)
{
    long total = 0;
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++) {
            double x = p.cx - p.w + 2.0 * p.w * (i + 0.5) / nx;
            double y = p.cy - p.w + 2.0 * p.w * (j + 0.5) / ny;
            int r, it = newton_point(x, y, p.m, &r);
            root[(size_t)j * nx + i] = (signed char)r;
            iters[(size_t)j * nx + i] = (unsigned short)it;
            total += it;
        }
    return total;
}

/* TODO: параллельная версия */
// static long basins_par(variant_t p, int nx, int ny, signed char *root, unsigned short *iters)
// {
//     long total = 0;
//     for (int j = 0; j < ny; j++)
//         for (int i = 0; i < nx; i++) {
//             double x = p.cx - p.w + 2.0 * p.w * (i + 0.5) / nx;
//             double y = p.cy - p.w + 2.0 * p.w * (j + 0.5) / ny;
//             int r, it = newton_point(x, y, p.m, &r);
//             root[(size_t)j * nx + i] = (signed char)r;
//             iters[(size_t)j * nx + i] = (unsigned short)it;
//             total += it;
//         }
//     return total;
// }

static long basins_par(variant_t p, int nx, int ny, signed char *root, unsigned short *iters)
{
    long total = 0;
    #pragma omp parallel default(none) shared(p, nx, ny, root, iters, stderr) reduction(+:total)
    {
        double t_start = omp_get_wtime();
        #pragma omp for schedule(runtime) collapse(2) nowait
        // #pragma omp for schedule(runtime) nowait
        for (int j = 0; j < ny; j++)
            for (int i = 0; i < nx; i++) {
                double x = p.cx - p.w + 2.0 * p.w * (i + 0.5) / nx;
                double y = p.cy - p.w + 2.0 * p.w * (j + 0.5) / ny;
                int r, it = newton_point(x, y, p.m, &r);
                root[(size_t)j * nx + i] = (signed char)r;
                iters[(size_t)j * nx + i] = (unsigned short)it;
                total += it;
            }
        double t_end = omp_get_wtime();
        fprintf(stderr, "thread N= %d  TIME = %.6f\n",omp_get_thread_num(), t_end - t_start);
    }
    return total;
}

/* Цветная картинка: оттенок - корень, яркость - число итераций */
static void write_bmp(const char *fn, const signed char *root, const unsigned short *it, int w, int h)
{
    static const unsigned char pal[8][3] = {{230, 80, 60}, {60, 160, 230}, {90, 200, 90}, {240, 190, 50},
                                            {170, 90, 220}, {50, 200, 200}, {240, 120, 180}, {150, 150, 150}};
    int rowsz = (3 * w + 3) & ~3;
    unsigned fsz = 54u + (unsigned)rowsz * h;
    unsigned char hd[54] = {'B', 'M'};
    hd[2] = fsz; hd[3] = fsz >> 8; hd[4] = fsz >> 16; hd[5] = fsz >> 24;
    hd[10] = 54; hd[14] = 40;
    hd[18] = w; hd[19] = w >> 8; hd[20] = w >> 16; hd[21] = w >> 24;
    hd[22] = h; hd[23] = h >> 8; hd[24] = h >> 16; hd[25] = h >> 24;
    hd[26] = 1; hd[28] = 24;
    FILE *f = fopen(fn, "wb");
    if (!f) return;
    fwrite(hd, 1, 54, f);
    unsigned char *row = calloc(rowsz, 1);
    for (int y = 0; y < h; y++) {          /* BMP: снизу вверх, у нас y растёт вверх - совпадает */
        for (int x = 0; x < w; x++) {
            size_t k = (size_t)y * w + x;
            double shade = 1.0 - fmin(0.85, it[k] / 40.0);
            for (int c = 0; c < 3; c++)
                row[3 * x + 2 - c] = root[k] < 0 ? 0 : (unsigned char)(pal[root[k] % 8][c] * shade);
        }
        fwrite(row, 1, rowsz, f);
    }
    free(row);
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "использование: %s V NX NY [seq|par|check] [img]  |  %s V info\n", argv[0], argv[0]); return 1; }
    int V = atoi(argv[1]);
    if (V < 1 || V > 17) { fprintf(stderr, "V должен быть 1..17\n"); return 1; }
    variant_t p = variant(V);
    if (strcmp(argv[2], "info") == 0) {
        printf("V=%d: m=%d, окно [%.2f, %.2f] x [%.2f, %.2f]\n", V, p.m, p.cx - p.w, p.cx + p.w, p.cy - p.w, p.cy + p.w);
        return 0;
    }
    int nx = atoi(argv[2]), ny = (argc > 3) ? atoi(argv[3]) : nx;
    int use_seq = (argc > 4 && strcmp(argv[4], "seq") == 0);
    if (argc > 4 && strcmp(argv[4], "check") == 0) {
        size_t np = (size_t)nx * ny;
        signed char *r1 = malloc(np), *r2 = malloc(np);
        unsigned short *i1 = malloc(np * sizeof *i1), *i2 = malloc(np * sizeof *i2);
        long t1 = basins_seq(p, nx, ny, r1, i1), t2 = basins_par(p, nx, ny, r2, i2);
        long bad = (t1 != t2);
        for (size_t k = 0; k < np; k++) bad += (r1[k] != r2[k]) || (i1[k] != i2[k]);
        printf("check: несовпадений %ld\nRESULT %ld\n", bad, bad);
        free(r1); free(r2); free(i1); free(i2);
        return 0;
    }
    int img = (argc > 5 && strcmp(argv[5], "img") == 0);

    signed char *root = malloc((size_t)nx * ny);
    unsigned short *iters = malloc((size_t)nx * ny * sizeof *iters);
    if (!root || !iters) { fprintf(stderr, "мало памяти\n"); return 1; }

    double t0 = omp_get_wtime();
    long total = use_seq ? basins_seq(p, nx, ny, root, iters) : basins_par(p, nx, ny, root, iters);
    double t = omp_get_wtime() - t0;

    long cnt[8] = {0}, bad = 0;
    for (size_t k = 0; k < (size_t)nx * ny; k++) { if (root[k] < 0) bad++; else cnt[root[k]]++; }
    printf("V=%d m=%d %dx%d %s, потоков %d\n", V, p.m, nx, ny, use_seq ? "seq" : "par", omp_get_max_threads());
    for (int k = 0; k < p.m; k++) printf("  корень %d: %.2f%%\n", k, 100.0 * cnt[k] / ((double)nx * ny));
    printf("  не сошлось: %ld\n", bad);
    if (img) write_bmp("basins.bmp", root, iters, nx, ny);
    printf("RESULT %ld\nTIME %.6f\n", total, t);
    free(root); free(iters);
    return 0;
}
