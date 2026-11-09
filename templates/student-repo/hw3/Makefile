# Образец Makefile для ДЗ. Обычно его заменяет Makefile из starter/ соответствующего задания курса.
# Цели: all test tsan bench plots clean
include ../common.mk

SRCS  := $(wildcard src/*.c)
PROGS := $(patsubst src/%.c,build/%$(EXE),$(SRCS))

all: $(PROGS)
	@if [ -z "$(SRCS)" ]; then echo "$(notdir $(CURDIR)): исходников пока нет — скопируйте starter/ из материалов курса"; fi

# Опишите проверку корректности, например:
#   @$(call check_result,./build/prog$(EXE) 1000,<эталон>,1e-9)
test: all
	@if [ -z "$(SRCS)" ]; then echo "$(notdir $(CURDIR)): тестировать пока нечего"; \
	 else echo "$(notdir $(CURDIR)): опишите цель test в Makefile"; exit 1; fi

# Опишите прогон под TSan, например:
#   tsan: build/prog_tsan$(EXE)
#   	@$(call run_tsan,./build/prog_tsan$(EXE) 1000)
tsan:
	@if [ -z "$(SRCS)" ]; then echo "$(notdir $(CURDIR)): проверять пока нечего"; \
	 else echo "$(notdir $(CURDIR)): опишите цель tsan в Makefile"; exit 1; fi

bench:
	@echo "Опишите замеры, например:"
	@echo '  $$(PYTHON) ../tools/bench.py --cmd "./build/prog$$(EXE) {n}" --threads $$(THREADS) --set n=1000 --repeat $$(REPEAT) --out results/prog.csv'

plots:
	@echo "Опишите построение графиков: $$(PYTHON) ../tools/plot.py results/prog.csv --kind speedup -o plots/prog_speedup.png"

clean: clean-common

.PHONY: all test tsan bench plots clean
