# ДЗ1. Цели: all, test, tsan, bench, plots, clean
# Компилятор, флаги OpenMP и правила сборки - в ../common.mk (шаблон репозитория курса).
# В репозитории студента common.mk лежит уровнем выше; в материалах курса - в templates/.
COMMON_MK := $(firstword $(wildcard ../common.mk ../../../templates/student-repo/common.mk))
include $(COMMON_MK)

# Ваш вариант (номер в журнале):
V      ?= 13
# Сетка newton для замеров:
NX     ?= 2000
# Лучшее расписание по итогам эксперимента 1 (впишите своё):
BEST   ?= dynamic,1
SCHEDS ?= static;static,1;static,16;dynamic,1;dynamic,16;dynamic,256;guided
# THREADS (список потоков для замеров) и REPEAT задаются в common.mk; переопределите:
#   make bench THREADS=1,2,4,6,8,12,16
MAXT    = $(lastword $(subst $(comma), ,$(THREADS)))
comma  := ,

PROGS := newton particles

all: $(foreach p,$(PROGS),build/$(p)$(EXE))

# Корректность: параллельные версии против последовательных на 1, 2, 4 потоках
# и при разных расписаниях. newton: число несовпавших точек = 0;
# particles: относительное расхождение энергии и сил <= 1e-10.
test: all
	@for s in static static,3 dynamic,1 dynamic,7 guided; do \
	  echo "== OMP_SCHEDULE=$$s"; export OMP_SCHEDULE=$$s; \
	  $(call check_result,./build/newton$(EXE) $(V) 300 300 check,0,0); \
	  $(call check_result,./build/particles$(EXE) $(V) check-full 12,0,1e-10); \
	  $(call check_result,./build/particles$(EXE) $(V) check-tri 12,0,1e-10); \
	done

# Гонки: clang + ThreadSanitizer + Archer, уменьшенные размеры
tsan: build/newton_tsan$(EXE) build/particles_tsan$(EXE)
	@$(call run_tsan,./build/newton_tsan$(EXE) $(V) 120 120 check)
	@$(call run_tsan,./build/particles_tsan$(EXE) $(V) check-full 7)
	@$(call run_tsan,./build/particles_tsan$(EXE) $(V) check-tri 7)

bench: all
	@mkdir -p results
	# Эксперимент 1: расписания на максимальном числе потоков
	$(PYTHON) ../tools/bench.py --cmd "./build/newton$(EXE) $(V) {nx} {nx} par" --threads $(MAXT) \
	    --set nx=$(NX) --env OMP_SCHEDULE="$(SCHEDS)" --repeat $(REPEAT) --out results/newton_sched.csv
	# Эксперимент 2: размер порции для dynamic
	$(PYTHON) ../tools/bench.py --cmd "./build/newton$(EXE) $(V) {nx} {nx} par" --threads $(MAXT) \
	    --set nx=$(NX) --env OMP_SCHEDULE="dynamic,1;dynamic,4;dynamic,16;dynamic,64;dynamic,256;dynamic,1024" \
	    --repeat $(REPEAT) --out results/newton_chunk.csv
	# Эксперимент 3: масштабирование static и лучшего расписания
	$(PYTHON) ../tools/bench.py --cmd "./build/newton$(EXE) $(V) {nx} {nx} par" --threads $(THREADS) \
	    --set nx=$(NX) --env OMP_SCHEDULE="static;$(BEST)" --repeat $(REPEAT) --out results/newton_scaling.csv
	# Эксперимент 4: частицы, full против tri
	$(PYTHON) ../tools/bench.py --cmd "./build/particles$(EXE) $(V) {mode}" --threads $(THREADS) \
	    --set mode=full,tri --env OMP_SCHEDULE="static;dynamic,16;guided" --repeat $(REPEAT) --out results/particles.csv

plots:
	@mkdir -p plots
	$(PYTHON) ../tools/plot.py results/newton_sched.csv --kind time --x OMP_SCHEDULE -o plots/newton_sched.png
	$(PYTHON) ../tools/plot.py results/newton_chunk.csv --kind time --x OMP_SCHEDULE -o plots/newton_chunk.png
	$(PYTHON) ../tools/plot.py results/newton_scaling.csv --kind speedup --group OMP_SCHEDULE -o plots/newton_speedup.png
	$(PYTHON) ../tools/plot.py results/newton_scaling.csv --kind efficiency --group OMP_SCHEDULE -o plots/newton_eff.png
	$(PYTHON) ../tools/plot.py results/particles.csv --kind time --group mode --group OMP_SCHEDULE --logy -o plots/particles_time.png
	$(PYTHON) ../tools/plot.py results/particles.csv --kind speedup --group mode --group OMP_SCHEDULE -o plots/particles_speedup.png

# Таблицы для REPORT.md
tables:
	@for f in newton_sched newton_chunk; do echo "### $$f"; $(PYTHON) ../tools/plot.py results/$$f.csv --kind time --x OMP_SCHEDULE --table; done
	@echo "### newton_scaling"; $(PYTHON) ../tools/plot.py results/newton_scaling.csv --kind speedup --group OMP_SCHEDULE --table
	@echo "### particles"; $(PYTHON) ../tools/plot.py results/particles.csv --kind time --group mode --group OMP_SCHEDULE --table

clean: clean-common
	rm -f basins.bmp

.PHONY: all test tsan bench plots tables clean
