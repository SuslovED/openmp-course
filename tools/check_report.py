#!/usr/bin/env python3
"""Проверка комплектности сдачи ДЗ (запускается в CI и локально).

    python3 tools/check_report.py            # все каталоги hw*/ и project/
    python3 tools/check_report.py hw1 hw2    # только указанные

Каталог проверяется, только если из REPORT.md удалена строка «template:unfilled».
Ошибки (CI красный):
  - нет обязательных разделов REPORT.md или они пустые;
  - нет results/*.csv или они пустые;
  - в отчёте есть ссылки на картинки, которых нет;
  - SOURCES.md не заполнен.
Предупреждения (CI зелёный, но преподаватель увидит):
  - раздел «Прогноз» появился в истории git не раньше, чем первые результаты.
"""
import glob
import os
import re
import subprocess
import sys

REPORT_SECTIONS = ["Прогноз", "Стенд", "Методика", "Результаты", "Объяснение", "Выводы"]
SRC_SECTIONS = ["Источники", "Для чего", "ошиб", "проверял", "самостоятельно"]
MARKER = "template:unfilled"
IN_CI = os.environ.get("GITHUB_ACTIONS") == "true"

errors = 0
warnings = 0


def say(kind, path, msg):
    global errors, warnings
    if kind == "error":
        errors += 1
    else:
        warnings += 1
    if IN_CI:
        print(f"::{kind} file={path}::{msg}")
    else:
        print(f"  [{'ОШИБКА' if kind == 'error' else 'внимание'}] {path}: {msg}")


def strip_comments(text):
    return re.sub(r"<!--.*?-->", "", text, flags=re.S)


def sections(text):
    """{заголовок второго уровня: содержимое без комментариев}"""
    res = {}
    cur = None
    buf = []
    for line in text.splitlines():
        m = re.match(r"^##\s+(.*?)\s*$", line)
        if m and not line.startswith("###"):
            if cur is not None:
                res[cur] = "\n".join(buf)
            cur, buf = m.group(1), []
        elif cur is not None:
            buf.append(line)
    if cur is not None:
        res[cur] = "\n".join(buf)
    return {k: strip_comments(v).strip() for k, v in res.items()}


def find_section(secs, keyword):
    for title, body in secs.items():
        if keyword.lower() in title.lower():
            return title, body
    return None, None


def git(*args):
    try:
        return subprocess.run(["git", *args], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                              universal_newlines=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return None


def check_history(d, report_path):
    """Прогноз должен появиться в истории раньше первых результатов."""
    order = git("rev-list", "--reverse", "--topo-order", "HEAD")
    if not order:
        return
    if git("rev-parse", "--is-shallow-repository") == "true\n":
        say("warning", report_path, "история git неполная (shallow clone), порядок «прогноз → результаты» не проверен")
        return
    idx = {c: i for i, c in enumerate(order.split())}
    top = (git("rev-parse", "--show-toplevel") or "").strip()
    rel_report = os.path.relpath(os.path.abspath(report_path), top).replace(os.sep, "/")
    rel_results = os.path.relpath(os.path.abspath(os.path.join(d, "results")), top).replace(os.sep, "/")

    res_commits = (git("log", "--format=%H", "--diff-filter=A", "--", rel_results + "/*.csv") or "").split()
    res_idx = min((idx[c] for c in res_commits if c in idx), default=None)
    if res_idx is None:
        return  # результаты ещё не закоммичены
    prog_idx = None
    for c in (git("log", "--format=%H", "--", rel_report) or "").split():
        if c not in idx:
            continue
        text = git("show", f"{c}:{rel_report}") or ""
        _, body = find_section(sections(text), "Прогноз")
        if body:
            prog_idx = idx[c] if prog_idx is None else min(prog_idx, idx[c])
    if prog_idx is None:
        say("warning", report_path, "в истории git не найден коммит с заполненным разделом «Прогноз»")
    elif prog_idx >= res_idx:
        say("warning", report_path, "раздел «Прогноз» закоммичен не раньше первых результатов (results/*.csv). "
                                     "По правилам курса прогноз фиксируется отдельным коммитом ДО замеров")


def check_dir(d):
    report = os.path.join(d, "REPORT.md")
    if not os.path.exists(report):
        print(f"{d}: REPORT.md нет — пропускаю")
        return
    with open(report, encoding="utf-8") as f:
        text = f.read()
    if MARKER in text:
        print(f"{d}: отчёт ещё не сдан (есть строка «{MARKER}») — пропускаю")
        return
    print(f"{d}: проверяю")

    secs = sections(text)
    for kw in REPORT_SECTIONS:
        title, body = find_section(secs, kw)
        if title is None:
            say("error", report, f"нет раздела «## … {kw} …»")
        elif len(body) < 20:
            say("error", report, f"раздел «{title}» пуст или слишком короткий")

    csvs = sorted(glob.glob(os.path.join(d, "results", "*.csv")))
    if not csvs:
        say("error", os.path.join(d, "results"), "нет сырых данных results/*.csv")
    for c in csvs:
        with open(c, encoding="utf-8") as f:
            lines = [l for l in f if l.strip()]
        if len(lines) < 2:
            say("error", c, "CSV пуст (нет строк с данными)")

    for img in re.findall(r"!\[[^\]]*\]\(([^)\s]+)", strip_comments(text)):
        if re.match(r"^[a-z]+://", img):
            continue
        if not os.path.exists(os.path.join(d, img)):
            say("error", report, f"картинка {img} не найдена (запустите make plots и закоммитьте plots/)")

    src = os.path.join(d, "SOURCES.md")
    if not os.path.exists(src):
        say("error", src, "нет файла SOURCES.md (источники и инструменты)")
    else:
        with open(src, encoding="utf-8") as f:
            stext = f.read()
        if MARKER in stext:
            say("error", src, f"SOURCES.md не заполнен (удалите строку «{MARKER}» после заполнения)")
        else:
            ssecs = sections(stext)
            _, src_body = find_section(ssecs, "Источники")
            only_course = bool(src_body) and re.search(r"только\s+материал|ничего,?\s+кроме", src_body, re.I)
            for kw in SRC_SECTIONS:
                title, body = find_section(ssecs, kw)
                if title is None:
                    say("error", src, f"нет раздела «## … {kw} …»")
                elif not body and not (only_course and kw in ("Для чего", "ошиб")):
                    say("error", src, f"раздел «{title}» пуст")

    check_history(d, report)


def main():
    dirs = sys.argv[1:] or sorted(glob.glob("hw*/")) + (["project/"] if os.path.isdir("project") else [])
    dirs = [x.rstrip("/\\") for x in dirs]
    for d in dirs:
        check_dir(d)
    print(f"\nИтого: ошибок {errors}, предупреждений {warnings}")
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
