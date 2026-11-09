#!/usr/bin/env bash
# Паспорт стенда: собирает характеристики машины и компилятора в SYSTEM.md.
# Linux, WSL, macOS. Использование:
#     bash tools/sysinfo.sh            # пишет SYSTEM.md в текущий каталог
#     bash tools/sysinfo.sh -o путь.md
#     CC=gcc-14 bash tools/sysinfo.sh  # проверить конкретный компилятор
set -u
OUT="SYSTEM.md"
if [ "${1:-}" = "-o" ] && [ -n "${2:-}" ]; then OUT="$2"; fi

OS="$(uname -s)"
have() { command -v "$1" >/dev/null 2>&1; }
val() { [ -n "${1:-}" ] && echo "$1" || echo "не удалось определить"; }

CPU_MODEL=""; PHYS=""; LOGICAL=""; PCORES=""; ECORES=""; L1D=""; L2=""; L3=""; RAM=""; OSNAME=""; POWER=""; FREQ=""; SOCKETS=""
WSL="нет"

if [ "$OS" = "Linux" ]; then
  grep -qiE 'microsoft|wsl' /proc/version 2>/dev/null && WSL="да"
  CPU_MODEL="$(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2- | sed 's/^ *//')"
  [ -z "$CPU_MODEL" ] && CPU_MODEL="$(lscpu 2>/dev/null | awk -F: '/Model name/{sub(/^ +/,"",$2); print $2; exit}')"
  LOGICAL="$(nproc --all 2>/dev/null || getconf _NPROCESSORS_ONLN)"
  if have lscpu; then
    PHYS="$(lscpu -p=CORE,SOCKET 2>/dev/null | grep -v '^#' | sort -u | wc -l)"
    SOCKETS="$(lscpu -p=SOCKET 2>/dev/null | grep -v '^#' | sort -u | wc -l)"
    L1D="$(lscpu 2>/dev/null | awk -F: '/L1d/{sub(/^ +/,"",$2); print $2; exit}')"
    L2="$(lscpu 2>/dev/null | awk -F: '/L2/{sub(/^ +/,"",$2); print $2; exit}')"
    L3="$(lscpu 2>/dev/null | awk -F: '/L3/{sub(/^ +/,"",$2); print $2; exit}')"
    FREQ="$(lscpu 2>/dev/null | awk -F: '/CPU max MHz/{sub(/^ +/,"",$2); printf "до %.0f МГц", $2; exit}')"
  fi
  # Гибридные процессоры Intel (P/E): разные максимальные частоты ядер или sysfs cpu_core/cpu_atom
  if [ -r /sys/devices/cpu_core/cpus ] && [ -r /sys/devices/cpu_atom/cpus ]; then
    cnt() { echo "$1" | tr ',' '\n' | awk -F- '{ if (NF==2) s+=$2-$1+1; else if ($1!="") s+=1 } END{print s+0}'; }
    PCORES="$(cnt "$(cat /sys/devices/cpu_core/cpus)") логических"
    ECORES="$(cnt "$(cat /sys/devices/cpu_atom/cpus)") логических"
  else
    PCORES="нет (не гибридный процессор) или не определено"; ECORES="$PCORES"
  fi
  RAM="$(awk '/MemTotal/{printf "%.1f ГБ", $2/1048576}' /proc/meminfo 2>/dev/null)"
  if [ -r /etc/os-release ]; then OSNAME="$(. /etc/os-release; echo "$PRETTY_NAME")"; fi
  OSNAME="$OSNAME, ядро $(uname -r)"
  [ "$WSL" = "да" ] && OSNAME="$OSNAME (WSL)"
  GOV="$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null)"
  [ -n "$GOV" ] && POWER="governor: $GOV"
  if have powerprofilesctl; then POWER="$POWER; power-profile: $(powerprofilesctl get 2>/dev/null)"; fi
  [ "$WSL" = "да" ] && POWER="WSL: смотрите план электропитания Windows (tools/sysinfo.ps1)"
elif [ "$OS" = "Darwin" ]; then
  CPU_MODEL="$(sysctl -n machdep.cpu.brand_string 2>/dev/null)"
  LOGICAL="$(sysctl -n hw.logicalcpu 2>/dev/null)"
  PHYS="$(sysctl -n hw.physicalcpu 2>/dev/null)"
  NLEV="$(sysctl -n hw.nperflevels 2>/dev/null || echo 0)"
  if [ "${NLEV:-0}" -ge 2 ]; then
    PCORES="$(sysctl -n hw.perflevel0.physicalcpu 2>/dev/null) физических"
    ECORES="$(sysctl -n hw.perflevel1.physicalcpu 2>/dev/null) физических"
    L2="P: $(( $(sysctl -n hw.perflevel0.l2cachesize 2>/dev/null || echo 0) / 1048576 )) МБ, E: $(( $(sysctl -n hw.perflevel1.l2cachesize 2>/dev/null || echo 0) / 1048576 )) МБ"
  else
    L2="$(( $(sysctl -n hw.l2cachesize 2>/dev/null || echo 0) / 1024 )) КБ"
  fi
  L1D="$(( $(sysctl -n hw.l1dcachesize 2>/dev/null || echo 0) / 1024 )) КБ на ядро"
  L3B="$(sysctl -n hw.l3cachesize 2>/dev/null || echo 0)"; [ "$L3B" -gt 0 ] 2>/dev/null && L3="$(( L3B / 1048576 )) МБ"
  RAM="$(( $(sysctl -n hw.memsize) / 1073741824 )) ГБ"
  OSNAME="macOS $(sw_vers -productVersion 2>/dev/null) ($(uname -m))"
  POWER="$(pmset -g 2>/dev/null | awk '/lowpowermode|powermode/{print $1"="$2}' | tr '\n' ' ')"
  [ -n "$POWER" ] || POWER="см. Настройки → Аккумулятор → Режим энергопотребления"
else
  OSNAME="$OS (возможно, MSYS2/Cygwin — для Windows используйте tools/sysinfo.ps1)"
  LOGICAL="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo "$NUMBER_OF_PROCESSORS")"
fi

# ---- Компилятор и версия OpenMP ----
CCX="${CC:-}"
if [ -z "$CCX" ]; then
  if [ "$OS" = "Darwin" ]; then
    for v in 15 14 13 12; do have "gcc-$v" && { CCX="gcc-$v"; break; }; done
    [ -z "$CCX" ] && CCX="clang"
  else
    CCX="gcc"; have gcc || CCX="cc"
  fi
fi
CC_VER="$($CCX --version 2>/dev/null | head -1)"
OMPFLAGS="-fopenmp"
if [ "$OS" = "Darwin" ] && $CCX --version 2>/dev/null | grep -q "Apple clang"; then
  LIBOMP="$(brew --prefix libomp 2>/dev/null)"
  OMPFLAGS="-Xpreprocessor -fopenmp -I$LIBOMP/include -L$LIBOMP/lib -lomp"
fi
TMPD="$(mktemp -d 2>/dev/null || echo /tmp/sysinfo.$$)"; mkdir -p "$TMPD"
cat > "$TMPD/v.c" <<'EOF'
#include <stdio.h>
#include <omp.h>
int main(void) {
#ifdef _OPENMP
    printf("%d %d %d\n", _OPENMP, omp_get_num_procs(), omp_get_max_threads());
#else
    printf("0 0 0\n");
#endif
    return 0;
}
EOF
OMP_LINE=""
if $CCX $OMPFLAGS "$TMPD/v.c" -o "$TMPD/v" >/dev/null 2>"$TMPD/err"; then
  OMP_LINE="$("$TMPD/v")"
else
  OMP_ERR="$(head -3 "$TMPD/err")"
fi
OMP_DATE="$(echo "$OMP_LINE" | awk '{print $1}')"
case "$OMP_DATE" in
  200505) OMP_VER="2.5";; 200805) OMP_VER="3.0";; 201107) OMP_VER="3.1";;
  201307) OMP_VER="4.0";; 201511) OMP_VER="4.5";; 201811) OMP_VER="5.0";;
  202011) OMP_VER="5.1";; 202111) OMP_VER="5.2";; 202411) OMP_VER="6.0";;
  "") OMP_VER="компиляция с OpenMP не удалась: ${OMP_ERR:-}";;
  *) OMP_VER="неизвестно";;
esac
OMP_PROCS="$(echo "$OMP_LINE" | awk '{print $2}')"
OMP_MAXT="$(echo "$OMP_LINE" | awk '{print $3}')"
CLANG_VER="$(clang --version 2>/dev/null | head -1)"
rm -rf "$TMPD"

{
  echo "# Паспорт стенда"
  echo
  echo "_Сгенерировано \`tools/sysinfo.sh\` $(date '+%Y-%m-%d %H:%M')._ Проверьте и дополните строки с пометкой «вручную»."
  echo
  echo "| Параметр | Значение |"
  echo "|---|---|"
  echo "| Модель ноутбука/ПК (вручную) | |"
  echo "| Процессор | $(val "$CPU_MODEL") |"
  echo "| Физических ядер | $(val "$PHYS") |"
  echo "| Логических процессоров (с SMT) | $(val "$LOGICAL") |"
  echo "| Сокетов / NUMA-узлов | $(val "$SOCKETS") |"
  echo "| Производительные ядра (P) | $(val "$PCORES") |"
  echo "| Энергоэффективные ядра (E) | $(val "$ECORES") |"
  echo "| Частота | $(val "$FREQ") |"
  echo "| Кэш L1d | $(val "$L1D") |"
  echo "| Кэш L2 | $(val "$L2") |"
  echo "| Кэш L3 | $(val "$L3") |"
  echo "| ОЗУ | $(val "$RAM") |"
  echo "| ОС | $(val "$OSNAME") |"
  echo "| Компилятор | $(val "$CC_VER") (\`$CCX $OMPFLAGS\`) |"
  echo "| Версия OpenMP (_OPENMP) | ${OMP_DATE:-?} → $OMP_VER |"
  echo "| omp_get_num_procs / omp_get_max_threads | ${OMP_PROCS:-?} / ${OMP_MAXT:-?} |"
  echo "| clang (для TSan) | $(val "$CLANG_VER") |"
  echo "| Режим питания | $(val "$POWER") |"
  echo "| Питание при замерах (вручную: сеть/батарея) | |"
  echo "| Пиковая полоса памяти по STREAM, ГБ/с (заполнить в ДЗ3) | |"
  echo
  echo "Если P/E-ядра не определились автоматически, найдите модель процессора на сайте производителя и впишите вручную."
} > "$OUT"
echo "sysinfo.sh: записано в $OUT"
cat "$OUT"
