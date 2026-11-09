# Паспорт стенда для Windows: собирает характеристики машины в SYSTEM.md.
# Запуск из PowerShell в корне репозитория:
#     powershell -ExecutionPolicy Bypass -File tools\sysinfo.ps1
#     powershell -ExecutionPolicy Bypass -File tools\sysinfo.ps1 -Out SYSTEM.md -CC gcc
# Если вы работаете в WSL, дополнительно запустите внутри WSL: bash tools/sysinfo.sh
# (компилятор и OpenMP там свои), а сюда возьмите данные о железе и электропитании.
param(
    [string]$Out = "SYSTEM.md",
    [string]$CC = ""
)
$ErrorActionPreference = "SilentlyContinue"

function Val($v) { if ($v -ne $null -and "$v" -ne "") { "$v" } else { "не удалось определить" } }

$cpu  = Get-CimInstance Win32_Processor | Select-Object -First 1
$cpus = @(Get-CimInstance Win32_Processor)
$cs   = Get-CimInstance Win32_ComputerSystem
$os   = Get-CimInstance Win32_OperatingSystem

$phys    = ($cpus | Measure-Object -Property NumberOfCores -Sum).Sum
$logical = ($cpus | Measure-Object -Property NumberOfLogicalProcessors -Sum).Sum
$sockets = $cpus.Count
$model   = $cpu.Name.Trim()
$maxMHz  = $cpu.MaxClockSpeed
$l2 = if ($cpu.L2CacheSize) { "$([math]::Round($cpu.L2CacheSize/1024,1)) МБ (суммарно)" } else { $null }
$l3 = if ($cpu.L3CacheSize) { "$([math]::Round($cpu.L3CacheSize/1024,1)) МБ" } else { $null }
$ram = "$([math]::Round($cs.TotalPhysicalMemory/1GB,1)) ГБ"
$osName = "$($os.Caption) $($os.Version) ($($os.OSArchitecture))"

# L1d через Win32_CacheMemory (Level=3 означает L1 в нумерации WMI)
$l1 = $null
$c1 = Get-CimInstance Win32_CacheMemory | Where-Object { $_.Level -eq 3 }
if ($c1) { $l1 = "$((($c1 | Measure-Object -Property MaxCacheSize -Sum).Sum)) КБ (суммарно L1)" }

# Гибридные процессоры (P/E): EfficiencyClass логических процессоров
$pcores = $null; $ecores = $null
try {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class CpuSet {
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool GetSystemCpuSetInformation(IntPtr info, uint len, out uint retLen, IntPtr proc, uint flags);
    public static int[] Classes() {
        uint len;
        GetSystemCpuSetInformation(IntPtr.Zero, 0, out len, IntPtr.Zero, 0);
        IntPtr buf = Marshal.AllocHGlobal((int)len);
        try {
            if (!GetSystemCpuSetInformation(buf, len, out len, IntPtr.Zero, 0)) return new int[0];
            var res = new System.Collections.Generic.List<int>();
            int off = 0;
            while (off < len) {
                int size = Marshal.ReadInt32(buf, off);
                // SYSTEM_CPU_SET_INFORMATION: Size(4) Type(4) Id(4) Group(2) LogicalProcessorIndex(1) CoreIndex(1)
                // LastLevelCacheIndex(1) NumaNodeIndex(1) EfficiencyClass(1)
                res.Add(Marshal.ReadByte(buf, off + 18));
                off += size;
            }
            return res.ToArray();
        } finally { Marshal.FreeHGlobal(buf); }
    }
}
"@ -ErrorAction Stop
    $classes = [CpuSet]::Classes()
    $distinct = $classes | Sort-Object -Unique
    if ($distinct.Count -ge 2) {
        $maxc = ($distinct | Measure-Object -Maximum).Maximum
        $pcores = "$(@($classes | Where-Object { $_ -eq $maxc }).Count) логических"
        $ecores = "$(@($classes | Where-Object { $_ -ne $maxc }).Count) логических"
    } elseif ($distinct.Count -eq 1) {
        $pcores = "нет (не гибридный процессор)"; $ecores = $pcores
    }
} catch { }

# План электропитания
$power = $null
$pl = powercfg /getactivescheme 2>$null
if ($pl) { $power = ($pl -replace '^.*\(', '' -replace '\).*$', '') }
$battery = Get-CimInstance Win32_Battery
if ($battery) {
    $ac = if ($battery.BatteryStatus -eq 2) { "от сети" } else { "от батареи (!)" }
    $power = "$power; сейчас питание $ac"
}

# Компилятор и версия OpenMP
if ($CC -eq "") { foreach ($c in @("gcc", "clang", "cc")) { if (Get-Command $c) { $CC = $c; break } } }
$ccVer = $null; $ompDate = $null; $ompProcs = $null; $ompMax = $null; $ompVer = $null
if ($CC -ne "" -and (Get-Command $CC)) {
    $ccVer = (& $CC --version 2>$null | Select-Object -First 1)
    $tmp = Join-Path $env:TEMP ("sysinfo_" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $tmp | Out-Null
    $src = Join-Path $tmp "v.c"; $exe = Join-Path $tmp "v.exe"
    @"
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
"@ | Set-Content -Path $src -Encoding ASCII
    & $CC -fopenmp $src -o $exe 2>$null
    if (Test-Path $exe) {
        $line = (& $exe).Trim() -split '\s+'
        $ompDate = $line[0]; $ompProcs = $line[1]; $ompMax = $line[2]
    }
    Remove-Item -Recurse -Force $tmp
}
$map = @{ "200505"="2.5"; "200805"="3.0"; "201107"="3.1"; "201307"="4.0"; "201511"="4.5";
          "201811"="5.0"; "202011"="5.1"; "202111"="5.2"; "202411"="6.0" }
if ($ompDate) { $ompVer = $map["$ompDate"]; if (-not $ompVer) { $ompVer = "неизвестно" } }
else { $ompVer = "компилятор с OpenMP не найден в PATH (это нормально, если вы работаете в WSL)" }

$now = Get-Date -Format "yyyy-MM-dd HH:mm"
$md = @"
# Паспорт стенда

_Сгенерировано ``tools/sysinfo.ps1`` $now._ Проверьте и дополните строки с пометкой «вручную».

| Параметр | Значение |
|---|---|
| Модель ноутбука/ПК (вручную) | $($cs.Manufacturer) $($cs.Model) |
| Процессор | $(Val $model) |
| Физических ядер | $(Val $phys) |
| Логических процессоров (с SMT) | $(Val $logical) |
| Сокетов / NUMA-узлов | $(Val $sockets) |
| Производительные ядра (P) | $(Val $pcores) |
| Энергоэффективные ядра (E) | $(Val $ecores) |
| Частота | до $(Val $maxMHz) МГц (номинальная по WMI) |
| Кэш L1d | $(Val $l1) |
| Кэш L2 | $(Val $l2) |
| Кэш L3 | $(Val $l3) |
| ОЗУ | $(Val $ram) |
| ОС | $(Val $osName) |
| Компилятор | $(Val $ccVer) |
| Версия OpenMP (_OPENMP) | $(Val $ompDate) → $ompVer |
| omp_get_num_procs / omp_get_max_threads | $(Val $ompProcs) / $(Val $ompMax) |
| Режим питания | $(Val $power) |
| Питание при замерах (вручную: сеть/батарея) | |
| Пиковая полоса памяти по STREAM, ГБ/с (заполнить в ДЗ3) | |

Если вы собираете и запускаете программы в WSL, добавьте ниже вывод ``bash tools/sysinfo.sh`` из WSL.
"@
$md | Out-File -FilePath $Out -Encoding utf8
Write-Host "sysinfo.ps1: записано в $Out"
Get-Content $Out -Encoding UTF8
