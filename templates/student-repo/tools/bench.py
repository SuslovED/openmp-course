#!/usr/bin/env python3
"""Серия замеров параллельной программы -> CSV.

Перебирает декартово произведение: число потоков x параметры (--set) x
переменные окружения (--env), запускает команду --repeat раз, разбирает из
вывода строки «RESULT <число>» и «TIME <секунды>» и пишет CSV:

    threads,<параметры>,<env>,rep,time,result

Пример:
    python3 tools/bench.py --cmd "./build/pi {n}" --threads 1,2,4,8 \
        --set n=10000000,100000000 --env OMP_SCHEDULE="static;dynamic,1" \
        --repeat 5 --out results/pi.csv

Рядом с CSV пишется <имя>.meta.json: машина, дата, команда, окружение.
Только стандартная библиотека Python 3.8+. Работает на Linux, macOS, Windows.
"""
import argparse
import csv
import datetime
import itertools
import json
import os
import platform
import re
import shlex
import subprocess
import sys
import time

RE_TIME = re.compile(r"^\s*TIME\s+([-+0-9.eE]+|nan|inf)\s*$", re.M)
RE_RESULT = re.compile(r"^\s*RESULT\s+(\S+)\s*$", re.M)

OMP_ENV_KEYS = ("OMP_NUM_THREADS", "OMP_SCHEDULE", "OMP_PROC_BIND", "OMP_PLACES",
                "OMP_DYNAMIC", "OMP_WAIT_POLICY", "OMP_STACKSIZE", "OMP_MAX_ACTIVE_LEVELS",
                "GOMP_CPU_AFFINITY", "KMP_AFFINITY")


def parse_int_list(s):
    out = []
    for part in s.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part and not part.startswith("-"):   # диапазон 1-8
            a, b = part.split("-", 1)
            out.extend(range(int(a), int(b) + 1))
        else:
            out.append(int(part))
    return out


def parse_set(items):
    """--set n=100,200 --set m=3 -> [('n', ['100','200']), ('m', ['3'])]"""
    res = []
    for it in items or []:
        if "=" not in it:
            sys.exit(f"bench.py: --set ожидает имя=значение1,значение2 (получено: {it!r})")
        k, v = it.split("=", 1)
        vals = [x.strip() for x in v.split(",") if x.strip() != ""]
        if not vals:
            sys.exit(f"bench.py: пустой список значений в --set {it!r}")
        res.append((k.strip(), vals))
    return res


def parse_env(items):
    """--env OMP_SCHEDULE="static;dynamic,1" -> [('OMP_SCHEDULE', ['static','dynamic,1'])]"""
    res = []
    for it in items or []:
        if "=" not in it:
            sys.exit(f"bench.py: --env ожидает ИМЯ=a;b;c (получено: {it!r})")
        k, v = it.split("=", 1)
        vals = [x.strip() for x in v.split(";") if x.strip() != ""]
        if not vals:
            sys.exit(f"bench.py: пустой список значений в --env {it!r}")
        res.append((k.strip(), vals))
    return res


def build_argv(template, subst):
    """Разбивает шаблон команды на аргументы и подставляет {параметры}.

    Подстановка выполняется после разбиения, поэтому значения с пробелами и
    запятыми безопасны. На Windows используем posix=False, чтобы обратные
    слеши в путях (build\\prog.exe) не съедались."""
    posix = os.name != "nt"
    parts = shlex.split(template, posix=posix)
    argv = []
    for p in parts:
        if not posix and len(p) >= 2 and p[0] == p[-1] == '"':
            p = p[1:-1]
        try:
            argv.append(p.format(**subst))
        except KeyError as e:
            sys.exit(f"bench.py: в --cmd используется {{{e.args[0]}}}, но такого параметра нет "
                     f"(доступны: {', '.join(sorted(subst))})")
    if argv and not os.path.isabs(argv[0]) and os.sep not in argv[0] and "/" not in argv[0]:
        # «prog» без пути: если файл есть в текущем каталоге — запускаем его
        if os.path.exists(argv[0]) or os.path.exists(argv[0] + ".exe"):
            argv[0] = os.path.join(".", argv[0])
    return argv


def run_once(argv, env, timeout):
    t0 = time.perf_counter()
    try:
        p = subprocess.run(argv, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           timeout=timeout, universal_newlines=True)
    except FileNotFoundError:
        sys.exit(f"bench.py: не найдена программа {argv[0]!r}. Соберите её (make) "
                 f"и проверьте путь в --cmd.")
    except subprocess.TimeoutExpired:
        return None, None, time.perf_counter() - t0, "timeout", ""
    wall = time.perf_counter() - t0
    out = p.stdout or ""
    m_t = RE_TIME.findall(out)
    m_r = RE_RESULT.findall(out)
    t = float(m_t[-1]) if m_t else None
    r = m_r[-1] if m_r else ""
    status = "ok" if p.returncode == 0 else f"exit {p.returncode}"
    return t, r, wall, status, (p.stderr or "")


def main():
    ap = argparse.ArgumentParser(
        description="Серия замеров OpenMP-программы с записью в CSV.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)
    ap.add_argument("--cmd", required=True,
                    help='команда с подстановками, напр. "./build/pi {n}"; доступны {threads} и имена из --set')
    ap.add_argument("--threads", default="1",
                    help="список чисел потоков: 1,2,4,8 или диапазон 1-8 (по умолчанию 1)")
    ap.add_argument("--set", action="append", metavar="ИМЯ=v1,v2",
                    help="параметр программы и его значения (можно несколько раз)")
    ap.add_argument("--env", action="append", metavar='ИМЯ="a;b"',
                    help="переменная окружения и её значения через ; (можно несколько раз)")
    ap.add_argument("--repeat", type=int, default=5, help="число повторов каждого замера (5)")
    ap.add_argument("--warmup", type=int, default=1,
                    help="число прогревочных запусков перед каждой конфигурацией (1), в CSV не пишутся")
    ap.add_argument("--out", required=True, help="CSV-файл результатов")
    ap.add_argument("--append", action="store_true", help="дописать в существующий CSV")
    ap.add_argument("--timeout", type=float, default=600, help="таймаут одного запуска, с (600)")
    ap.add_argument("--shuffle", action="store_true",
                    help="перемешать порядок конфигураций (меньше систематических ошибок от нагрева)")
    args = ap.parse_args()

    threads = parse_int_list(args.threads)
    if not threads:
        sys.exit("bench.py: пустой --threads")
    params = parse_set(args.set)
    envs = parse_env(args.env)
    if args.repeat < 1:
        sys.exit("bench.py: --repeat должен быть >= 1")

    pnames = [k for k, _ in params]
    enames = [k for k, _ in envs]
    clash = set(pnames) & set(enames) | ({"threads", "rep", "time", "result"} & (set(pnames) | set(enames)))
    if clash:
        sys.exit(f"bench.py: конфликт имён столбцов: {', '.join(sorted(clash))}")
    header = ["threads"] + pnames + enames + ["rep", "time", "result"]

    out_dir = os.path.dirname(os.path.abspath(args.out))
    os.makedirs(out_dir, exist_ok=True)
    mode = "w"
    if args.append and os.path.exists(args.out) and os.path.getsize(args.out) > 0:
        with open(args.out, newline="", encoding="utf-8") as f:
            old = next(csv.reader(f), None)
        if old != header:
            sys.exit(f"bench.py: заголовок существующего {args.out} ({old}) не совпадает с новым "
                     f"({header}). Запишите в другой файл.")
        mode = "a"

    configs = list(itertools.product(threads,
                                     itertools.product(*[v for _, v in params]),
                                     itertools.product(*[v for _, v in envs])))
    if args.shuffle:
        import random
        random.shuffle(configs)

    total = len(configs) * args.repeat
    print(f"bench.py: {len(configs)} конфигураций x {args.repeat} повторов = {total} запусков",
          file=sys.stderr)

    base_env = dict(os.environ)
    warned_no_time = False
    n_fail = 0
    done = 0
    results_seen = {}

    with open(args.out, mode, newline="", encoding="utf-8") as f:
        w = csv.writer(f, lineterminator="\n")
        if mode == "w":
            w.writerow(header)
        for th, pvals, evals in configs:
            subst = dict(zip(pnames, pvals))
            subst["threads"] = th
            argv = build_argv(args.cmd, subst)
            env = dict(base_env)
            env["OMP_NUM_THREADS"] = str(th)
            for k, v in zip(enames, evals):
                env[k] = v
            label = " ".join([f"threads={th}"] + [f"{k}={v}" for k, v in zip(pnames, pvals)]
                             + [f"{k}={v}" for k, v in zip(enames, evals)])
            for _ in range(args.warmup):
                run_once(argv, env, args.timeout)
            for rep in range(1, args.repeat + 1):
                t, r, wall, status, err = run_once(argv, env, args.timeout)
                done += 1
                if status != "ok":
                    n_fail += 1
                    print(f"[{done}/{total}] {label} rep={rep}: ОШИБКА ({status})", file=sys.stderr)
                    if err.strip():
                        print("    stderr: " + err.strip().splitlines()[-1], file=sys.stderr)
                    w.writerow([th] + list(pvals) + list(evals) + [rep, "", status])
                    f.flush()
                    continue
                if t is None:
                    t = wall
                    if not warned_no_time:
                        print("bench.py: ПРЕДУПРЕЖДЕНИЕ: программа не печатает строку «TIME <с>», "
                              "записываю время всего процесса (включая запуск и инициализацию).",
                              file=sys.stderr)
                        warned_no_time = True
                key = tuple(pvals)
                if r != "":
                    results_seen.setdefault(key, set()).add(r)
                w.writerow([th] + list(pvals) + list(evals) + [rep, f"{t:.6g}", r])
                f.flush()
                print(f"[{done}/{total}] {label} rep={rep}: time={t:.4g} s"
                      + (f" result={r}" if r != "" else ""), file=sys.stderr)

    meta = {
        "created": datetime.datetime.now().isoformat(timespec="seconds"),
        "hostname": platform.node(),
        "platform": platform.platform(),
        "processor": platform.processor(),
        "cpu_count_logical": os.cpu_count(),
        "python": sys.version.split()[0],
        "cmd": args.cmd,
        "threads": threads,
        "set": {k: v for k, v in params},
        "env_sweep": {k: v for k, v in envs},
        "repeat": args.repeat,
        "warmup": args.warmup,
        "omp_env": {k: os.environ[k] for k in OMP_ENV_KEYS if k in os.environ},
        "argv": sys.argv,
    }
    meta_path = os.path.splitext(args.out)[0] + ".meta.json"
    if args.append and os.path.exists(meta_path):
        try:
            with open(meta_path, encoding="utf-8") as mf:
                old_meta = json.load(mf)
            runs = old_meta.get("runs", [old_meta])
        except (OSError, ValueError):
            runs = []
        runs.append(meta)
        meta = {"runs": runs}
    with open(meta_path, "w", encoding="utf-8") as mf:
        json.dump(meta, mf, ensure_ascii=False, indent=2)

    # Подсказка о корректности: разные RESULT при одинаковых параметрах
    for key, vals in results_seen.items():
        if len(vals) > 1:
            shown = ", ".join(sorted(vals)[:5])
            print(f"bench.py: ВНИМАНИЕ: при параметрах {dict(zip(pnames, key)) or '(без параметров)'} "
                  f"программа выдала разные RESULT: {shown}. Это гонка или неассоциативность "
                  f"сложения? Разберитесь.", file=sys.stderr)

    print(f"bench.py: записано в {args.out} (+ {os.path.basename(meta_path)})"
          + (f"; неудачных запусков: {n_fail}" if n_fail else ""), file=sys.stderr)
    sys.exit(1 if n_fail else 0)


if __name__ == "__main__":
    main()
