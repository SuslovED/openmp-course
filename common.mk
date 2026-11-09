# common.mk — общие настройки сборки для всех ДЗ курса OpenMP.
# Подключается из hwN/Makefile строкой «include ../common.mk».
#
# Автоматически определяет компилятор и флаги OpenMP:
#   Linux / WSL     : gcc -fopenmp
#   macOS           : brew gcc-NN, если установлен; иначе Apple clang + brew libomp
#   MSYS2 / MinGW   : gcc -fopenmp, программы с суффиксом .exe
# Всё можно переопределить из командной строки, например:
#   make CC=clang
#   make CC=gcc-14
#   make OMPFLAGS="-fopenmp" OMPLIBS=""
#
# Диагностика: make info

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
.DEFAULT_GOAL := all

EXE        :=
CSTD       ?= -std=c11
OPT        ?= -O2
WARN       ?= -Wall -Wextra
WERROR     ?=
CFLAGS     ?= $(OPT) $(CSTD) $(WARN) $(WERROR)
LDLIBS     ?= -lm
PYTHON     ?= python3
THREADS    ?= 1,2,4,8
REPEAT     ?= 5

# Ключи для ThreadSanitizer (нужен clang с libomp; gcc+TSan даёт ложные срабатывания)
TSAN_CC       ?= clang
TSAN_CFLAGS   ?= -g -O1 $(CSTD) -fsanitize=thread
TSAN_OMPFLAGS ?= -fopenmp
TSAN_OMPLIBS  ?=
TSAN_OPTIONS_RUN ?= ignore_noninstrumented_modules=1 halt_on_error=1 exitcode=66 history_size=4

ifeq ($(UNAME_S),Darwin)
  # ---------- macOS ----------
  ifeq ($(origin CC),default)
    BREW_GCC := $(shell for v in 15 14 13 12; do command -v gcc-$$v >/dev/null 2>&1 && { echo gcc-$$v; break; }; done)
    ifneq ($(BREW_GCC),)
      CC := $(BREW_GCC)
    else
      CC := clang
    endif
  endif
  ifneq ($(shell $(CC) --version 2>/dev/null | grep -c 'Apple clang'),0)
    LIBOMP_PREFIX := $(shell brew --prefix libomp 2>/dev/null)
    OMPFLAGS ?= -Xpreprocessor -fopenmp -I$(LIBOMP_PREFIX)/include
    OMPLIBS  ?= -L$(LIBOMP_PREFIX)/lib -lomp
  else
    OMPFLAGS ?= -fopenmp
    OMPLIBS  ?=
  endif
  # TSan на macOS. Лучший вариант — clang из brew llvm (в нём есть libomp и Archer).
  # Запасной — Apple clang + brew libomp (без Archer возможны ложные срабатывания в рантайме).
  # Основная проверка в любом случае выполняется в CI на Linux.
  BREW_LLVM := $(shell brew --prefix llvm 2>/dev/null)
  ifneq ($(and $(BREW_LLVM),$(wildcard $(BREW_LLVM)/bin/clang)),)
    TSAN_CC       := $(BREW_LLVM)/bin/clang
    TSAN_OMPFLAGS := -fopenmp
    TSAN_OMPLIBS  := -L$(BREW_LLVM)/lib -Wl,-rpath,$(BREW_LLVM)/lib
  else
    LIBOMP_PREFIX ?= $(shell brew --prefix libomp 2>/dev/null)
    TSAN_OMPFLAGS := -Xpreprocessor -fopenmp -I$(LIBOMP_PREFIX)/include
    TSAN_OMPLIBS  := -L$(LIBOMP_PREFIX)/lib -lomp
  endif
else ifneq (,$(findstring MINGW,$(UNAME_S))$(findstring UCRT,$(UNAME_S))$(findstring CLANG,$(UNAME_S))$(findstring MSYS,$(UNAME_S)))
  # ---------- Windows: MSYS2 / MinGW-w64 ----------
  EXE := .exe
  ifeq ($(origin CC),default)
    CC := gcc
  endif
  OMPFLAGS ?= -fopenmp
  OMPLIBS  ?=
  NO_TSAN  := 1
else
  # ---------- Linux / WSL ----------
  ifeq ($(origin CC),default)
    CC := gcc
  endif
  OMPFLAGS ?= -fopenmp
  OMPLIBS  ?=
endif

# Archer — модуль libomp, который сообщает TSan о синхронизации OpenMP (барьеры, задачи и т.д.).
# Без него TSan выдаёт ложные срабатывания внутри рантайма. Ищем библиотеку рядом с clang.
TSAN_RESOURCE_DIR := $(shell $(TSAN_CC) -print-resource-dir 2>/dev/null)
ARCHER_LIB ?= $(firstword $(wildcard \
    $(TSAN_RESOURCE_DIR)/../../libarcher.so \
    $(TSAN_RESOURCE_DIR)/../../libarcher.dylib \
    /usr/lib/llvm-*/lib/libarcher.so \
    /usr/local/lib/libarcher.so \
    /opt/homebrew/opt/llvm/lib/libarcher.dylib \
    /usr/local/opt/llvm/lib/libarcher.dylib))

# ---------- Общие правила ----------
build:
	@mkdir -p build

build/%$(EXE): src/%.c $(wildcard src/*.h) | build
	$(CC) $(CFLAGS) $(OMPFLAGS) $< -o $@ $(OMPLIBS) $(LDLIBS)

build/%_tsan$(EXE): src/%.c $(wildcard src/*.h) | build
ifdef NO_TSAN
	@echo "ThreadSanitizer недоступен в MSYS2/MinGW. Запустите make tsan в WSL или смотрите результат CI."; exit 1
else
	$(TSAN_CC) $(TSAN_CFLAGS) $(TSAN_OMPFLAGS) $< -o $@ $(TSAN_OMPLIBS) $(LDLIBS)
endif

# check_result: запустить программу на 1, 2, 4 потоках (TEST_THREADS) и сравнить RESULT с эталоном.
#   @$(call check_result,<команда>,<эталон>,<допуск>)
# Пример:  @$(call check_result,./build/pi$(EXE) 1000000,3.14159265358979,1e-9)
TEST_THREADS ?= 1 2 4
check_result = for t in $(TEST_THREADS); do \
	  out="$$(OMP_NUM_THREADS=$$t $(1))" || { echo "FAIL: '$(1)' на $$t потоках завершилась с ошибкой"; exit 1; }; \
	  echo "$$out" | awk -v ref="$(2)" -v tol="$(3)" -v cmd="$(1)" -v th="$$t" \
	    '/^RESULT[ \t]/{r=$$2; found=1} END{ if(!found){print "FAIL: нет строки RESULT ("cmd")"; exit 1} \
	     d=r-ref; if(d<0)d=-d; if(d>tol){printf "FAIL: %s, потоков %s: RESULT=%s, ожидалось %s ± %s\n", cmd, th, r, ref, tol; exit 1} \
	     printf "OK   %s, потоков %s: RESULT=%s\n", cmd, th, r}' || exit 1; \
	done

# run_tsan: прогон программы под TSan на 2 и 4 потоках
#   @$(call run_tsan,<команда>)
run_tsan = for t in 2 4; do \
	  echo "TSan: OMP_NUM_THREADS=$$t $(1)"; \
	  OMP_NUM_THREADS=$$t TSAN_OPTIONS="$(TSAN_OPTIONS_RUN)" $(if $(ARCHER_LIB),OMP_TOOL_LIBRARIES="$(ARCHER_LIB)") $(1) > /dev/null || \
	    { echo "TSan нашёл проблему (или программа упала) на $$t потоках"; exit 1; }; \
	done

info:
	@echo "ОС:          $(UNAME_S)"
	@echo "CC:          $(CC)  ($$($(CC) --version 2>/dev/null | head -1))"
	@echo "CFLAGS:      $(CFLAGS)"
	@echo "OMPFLAGS:    $(OMPFLAGS)"
	@echo "OMPLIBS:     $(OMPLIBS)"
	@echo "TSAN_CC:     $(TSAN_CC) $(TSAN_CFLAGS) $(TSAN_OMPFLAGS) $(TSAN_OMPLIBS)"
	@echo "ARCHER_LIB:  $(if $(ARCHER_LIB),$(ARCHER_LIB),не найден — возможны ложные срабатывания TSan в рантайме OpenMP)"
	@echo "EXE suffix:  '$(EXE)'"

clean-common:
	rm -rf build

.PHONY: info clean-common
