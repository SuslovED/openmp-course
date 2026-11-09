#!/usr/bin/env python3
"""Графики и таблицы по CSV из tools/bench.py.

Примеры:
    python3 tools/plot.py results/pi.csv --kind time -o plots/pi_time.png
    python3 tools/plot.py results/pi.csv --kind speedup --group OMP_SCHEDULE \
        --filter n=100000000 -o plots/pi_speedup.png
    python3 tools/plot.py results/pi.csv --kind efficiency --x threads -o plots/pi_eff.png
    python3 tools/plot.py results/pi.csv --kind speedup --table      # markdown-таблица

Ускорение S(p) = медиана T(1) / медиана T(p) внутри одной группы
(группа = значение --group + все прочие столбцы-параметры, кроме --x).
Эффективность E(p) = S(p) / p.
"""
import argparse
import csv
import math
import os
import statistics
import sys
from collections import OrderedDict

SERVICE = {"rep", "time", "result"}
KIND_LABEL = {"time": "Время, с", "speedup": "Ускорение S(p)", "efficiency": "Эффективность E(p)"}
X_LABEL = {"threads": "Число потоков p"}


def num(s):
    try:
        v = float(s)
        return int(v) if v.is_integer() and "." not in s and "e" not in s.lower() else v
    except (TypeError, ValueError):
        return s


def sort_key(v):
    n = num(v)
    return (0, n, "") if isinstance(n, (int, float)) else (1, 0, str(v))


def load(path, filters):
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        sys.exit(f"plot.py: {path} пуст")
    for k, v in filters:
        if k not in rows[0]:
            sys.exit(f"plot.py: столбца {k!r} нет в {path} (есть: {', '.join(rows[0])})")
        rows = [r for r in rows if r[k] == v]
    bad = [r for r in rows if r.get("time", "") in ("", None)]
    rows = [r for r in rows if r.get("time", "") not in ("", None)]
    if bad:
        print(f"plot.py: пропущено {len(bad)} строк с ошибкой запуска", file=sys.stderr)
    if not rows:
        sys.exit("plot.py: после фильтров не осталось данных")
    return rows


def quartiles(xs):
    xs = sorted(xs)
    if len(xs) < 4:
        return xs[0], xs[-1]
    q = statistics.quantiles(xs, n=4)
    return q[0], q[2]


def aggregate(rows, x, group_cols, spread):
    """-> OrderedDict[group_tuple] -> list of (xval, median, lo, hi, n)"""
    data = {}
    for r in rows:
        g = tuple(r[c] for c in group_cols)
        data.setdefault(g, {}).setdefault(r[x], []).append(float(r["time"]))
    out = OrderedDict()
    for g in sorted(data, key=lambda t: [sort_key(v) for v in t]):
        series = []
        for xv in sorted(data[g], key=sort_key):
            ts = data[g][xv]
            med = statistics.median(ts)
            lo, hi = (min(ts), max(ts)) if spread == "minmax" else quartiles(ts)
            series.append((xv, med, lo, hi, len(ts)))
        out[g] = series
    return out


def transform(series, kind, x):
    """Время -> ускорение/эффективность относительно точки x=1 (или минимального x)."""
    if kind == "time":
        return series, None
    base = None
    for xv, med, lo, hi, n in series:
        if str(xv) == "1":
            base = med
    base_x = "1"
    if base is None:
        xv, med, *_ = series[0]
        base, base_x = med, xv
    res = []
    for xv, med, lo, hi, n in series:
        s, s_lo, s_hi = base / med, base / hi, base / lo
        if kind == "efficiency":
            p = float(num(xv)) if x == "threads" else 1.0
            s, s_lo, s_hi = s / p, s_lo / p, s_hi / p
        res.append((xv, s, s_lo, s_hi, n))
    return res, base_x


def group_name(cols, g):
    if not cols:
        return "все данные"
    return ", ".join(f"{c}={v}" for c, v in zip(cols, g))


def print_table(agg, kind, x, group_cols):
    xs = sorted({xv for s in agg.values() for xv, *_ in s}, key=sort_key)
    title = {"time": "медиана времени, с", "speedup": "ускорение", "efficiency": "эффективность"}[kind]
    head = ["—" if not group_cols else " / ".join(group_cols)] + [f"{x}={v}" for v in xs]
    print(f"<!-- {title} -->")
    print("| " + " | ".join(head) + " |")
    print("|" + "---|" * len(head))
    for g, series in agg.items():
        tr, _ = transform(series, kind, x)
        d = {xv: v for xv, v, *_ in tr}
        cells = []
        for xv in xs:
            v = d.get(xv)
            cells.append("—" if v is None else (f"{v:.4g}" if kind == "time" else f"{v:.2f}"))
        name = " / ".join(g) if g else "—"
        print("| " + " | ".join([name] + cells) + " |")


def main():
    ap = argparse.ArgumentParser(description="Графики по CSV из bench.py",
                                 formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    ap.add_argument("csv")
    ap.add_argument("--kind", choices=["time", "speedup", "efficiency"], default="time")
    ap.add_argument("--x", default="threads", help="столбец по оси X (threads)")
    ap.add_argument("--group", action="append", default=[],
                    help="столбец, по которому строятся отдельные кривые (можно несколько раз)")
    ap.add_argument("--filter", action="append", default=[], metavar="ИМЯ=ЗНАЧЕНИЕ",
                    help="оставить только строки с этим значением (можно несколько раз)")
    ap.add_argument("--spread", choices=["minmax", "iqr", "none"], default="minmax",
                    help="разброс по повторам: min–max (по умолчанию), межквартильный, без")
    ap.add_argument("--logx", action="store_true")
    ap.add_argument("--logy", action="store_true")
    ap.add_argument("--no-ideal", action="store_true", help="не рисовать идеальное ускорение")
    ap.add_argument("--title", default=None)
    ap.add_argument("--xlabel", default=None)
    ap.add_argument("--table", action="store_true", help="напечатать markdown-таблицу вместо графика")
    ap.add_argument("-o", "--out", default=None, help="PNG-файл (по умолчанию plots/<имя csv>_<kind>.png)")
    args = ap.parse_args()

    filters = []
    for f in args.filter:
        if "=" not in f:
            sys.exit(f"plot.py: --filter ожидает ИМЯ=ЗНАЧЕНИЕ (получено {f!r})")
        k, v = f.split("=", 1)
        filters.append((k, v))
    rows = load(args.csv, filters)
    cols = list(rows[0].keys())
    if args.x not in cols:
        sys.exit(f"plot.py: столбца {args.x!r} нет (есть: {', '.join(cols)})")
    for g in args.group:
        if g not in cols:
            sys.exit(f"plot.py: столбца {g!r} нет (есть: {', '.join(cols)})")
    # Прочие варьируемые параметры тоже разделяют кривые, иначе медиана смешает разные n
    varying = [c for c in cols if c not in SERVICE and c != args.x and c not in args.group
               and len({r[c] for r in rows}) > 1]
    group_cols = args.group + varying
    if varying:
        print(f"plot.py: столбцы {', '.join(varying)} тоже меняются — строю для них отдельные кривые "
              f"(или отфильтруйте через --filter)", file=sys.stderr)

    agg = aggregate(rows, args.x, group_cols, args.spread)

    if args.table:
        print_table(agg, args.kind, args.x, group_cols)
        return

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(7, 4.5), dpi=130)
    numeric_x = all(isinstance(num(xv), (int, float)) for s in agg.values() for xv, *_ in s)
    max_x = 0
    base_note = None
    for g, series in agg.items():
        tr, base_x = transform(series, args.kind, args.x)
        if base_x is not None and base_x != "1":
            base_note = base_x
        xs = [num(xv) if numeric_x else str(xv) for xv, *_ in tr]
        ys = [v for _, v, *_ in tr]
        lo = [v - l for _, v, l, h, _ in tr]
        hi = [h - v for _, v, l, h, _ in tr]
        if numeric_x:
            max_x = max(max_x, max(xs))
        label = group_name(group_cols, g) if group_cols else None
        if args.spread == "none":
            ax.plot(xs, ys, marker="o", label=label)
        else:
            ax.errorbar(xs, ys, yerr=[lo, hi], marker="o", capsize=3, label=label)

    if args.kind in ("speedup", "efficiency") and args.x == "threads" and numeric_x and not args.no_ideal:
        ideal_x = [1, max_x]
        ideal_y = [1, max_x] if args.kind == "speedup" else [1, 1]
        ax.plot(ideal_x, ideal_y, "k--", linewidth=1, label="идеальное")
    if args.kind == "efficiency":
        ax.set_ylim(bottom=0)

    ax.set_xlabel(args.xlabel or X_LABEL.get(args.x, args.x))
    ax.set_ylabel(KIND_LABEL[args.kind])
    if numeric_x and args.x == "threads":
        ticks = sorted({num(xv) for s in agg.values() for xv, *_ in s})
        ax.set_xticks(ticks)
    if args.logx:
        ax.set_xscale("log", base=2)
    if args.logy:
        ax.set_yscale("log")
    title = args.title or os.path.splitext(os.path.basename(args.csv))[0]
    if filters:
        title += "  (" + ", ".join(f"{k}={v}" for k, v in filters) + ")"
    ax.set_title(title)
    ax.grid(True, alpha=0.3)
    spread_txt = {"minmax": "медиана, разброс min–max", "iqr": "медиана, разброс Q1–Q3", "none": "медиана"}
    note = spread_txt[args.spread]
    if base_note:
        note += f"; база ускорения: {args.x}={base_note}"
    ax.text(0.99, 0.01, note, transform=ax.transAxes, ha="right", va="bottom", fontsize=7, color="gray")
    if ax.get_legend_handles_labels()[0]:
        ax.legend(fontsize=8)
    fig.tight_layout()

    out = args.out or os.path.join("plots", f"{os.path.splitext(os.path.basename(args.csv))[0]}_{args.kind}.png")
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    fig.savefig(out)
    print(f"plot.py: сохранено {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
