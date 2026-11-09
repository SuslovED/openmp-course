# ДЗ0: проверка окружения. Цели: all test tsan bench plots clean
include ../common.mk

PROGS := hello pi

all: $(foreach p,$(PROGS),build/$(p)$(EXE))

# Корректность: pi на 1, 2, 4 потоках совпадает с эталоном
test: all
	@for t in 1 2 4; do OMP_NUM_THREADS=$$t ./build/hello$(EXE) > /dev/null || exit 1; done
	@$(call check_result,./build/pi$(EXE) 1000000,3.14159265358979,1e-9)

# Поиск гонок (clang + libomp/Archer)
tsan: build/pi_tsan$(EXE) build/hello_tsan$(EXE)
	@$(call run_tsan,./build/hello_tsan$(EXE))
	@$(call run_tsan,./build/pi_tsan$(EXE) 200000)

# Замеры: результаты в results/pi.csv
# Размер задачи N подобран так, чтобы на 1 потоке расчёт шёл ≈0,5–2 с (см. методику замеров).
# Если у вас на 1 потоке меньше 0,5 с или больше 5 с, поменяйте: make bench N=...
N ?= 400000000
bench: all
	$(PYTHON) ../tools/bench.py --cmd "./build/pi$(EXE) {n}" --threads $(THREADS) \
	    --set n=$(N) --repeat $(REPEAT) --out results/pi.csv

plots:
	$(PYTHON) ../tools/plot.py results/pi.csv --kind time      -o plots/pi_time.png
	$(PYTHON) ../tools/plot.py results/pi.csv --kind speedup   -o plots/pi_speedup.png
	$(PYTHON) ../tools/plot.py results/pi.csv --kind efficiency -o plots/pi_eff.png

clean: clean-common

.PHONY: all test tsan bench plots clean
