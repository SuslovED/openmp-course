# С1: примеры.  make — собрать всё;  make info — какой компилятор выбран;
# make run — таблица T(p);  make tsan — гонка под ThreadSanitizer (нужен clang + libomp)
#
# Компилятор выбирается сам: Linux, WSL, MSYS2 — gcc; macOS — gcc-NN из Homebrew,
# а если его нет — Apple clang + brew libomp. Вручную: make CC=gcc-15 или make CC=clang
UNAME_S := $(shell uname -s 2>/dev/null)
ifeq ($(origin CC),default)
  CC := gcc
  ifeq ($(UNAME_S),Darwin)
    CC := $(or $(shell for v in 15 14 13 12; do command -v gcc-$$v >/dev/null 2>&1 && { echo gcc-$$v; break; }; done),clang)
  endif
endif
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra
ifneq ($(shell $(CC) --version 2>/dev/null | grep -c 'Apple clang'),0)
  LIBOMP  := $(shell brew --prefix libomp 2>/dev/null)
  OMP     ?= -Xpreprocessor -fopenmp -I$(LIBOMP)/include
  OMPLIBS ?= -L$(LIBOMP)/lib -lomp
else
  OMP     ?= -fopenmp
endif
LDLIBS  = $(OMPLIBS) -lm
PROGS   = hello pi_serial pi_spmd_race pi_spmd_critical pi_spmd_array pi_local_atomic scope timing review_pi_bug

EXTRA   = $(patsubst %.c,%,$(wildcard practice/*.c solutions/*.c))

all: $(PROGS) $(EXTRA)

%: %.c
	$(CC) $(CFLAGS) $(OMP) $< -o $@ $(LDLIBS)

ARCHER ?= $(firstword $(wildcard /usr/lib/llvm-*/lib/libarcher.so))

# Демонстрация гонки: clang + Archer
tsan: pi_spmd_race.c
	clang -O1 -g -std=c11 -fopenmp -fsanitize=thread pi_spmd_race.c -o pi_spmd_race_tsan -lm
	TSAN_OPTIONS=ignore_noninstrumented_modules=1 OMP_TOOL_LIBRARIES=$(ARCHER) OMP_NUM_THREADS=2 ./pi_spmd_race_tsan 100000 || true

# Таблица T(p) для всех версий
run: all
	@for p in 1 2 4 8; do echo "== OMP_NUM_THREADS=$$p"; \
	  for x in pi_spmd_race pi_spmd_array pi_local_atomic; do \
	    printf "%-18s" $$x; OMP_NUM_THREADS=$$p ./$$x | grep -E '^(RESULT|TIME)' | tr '\n' ' '; echo; done; done

info:
	@echo "ОС:   $(UNAME_S)"
	@echo "CC:   $(CC)  ($$($(CC) --version 2>/dev/null | head -1))"
	@echo "OMP:  $(OMP) $(OMPLIBS)"

clean:
	rm -f $(PROGS) $(EXTRA) pi_spmd_race_tsan review_pi_bug_asan asan.log

# AddressSanitizer для разбора чужого кода
asan: review_pi_bug.c
	$(CC) -O1 -g -std=c11 $(OMP) -fsanitize=address review_pi_bug.c -o review_pi_bug_asan $(OMPLIBS)
	-OMP_NUM_THREADS=4 ./review_pi_bug_asan 1000000 > asan.log 2>&1
	@head -12 asan.log

.PHONY: all tsan run info clean asan
