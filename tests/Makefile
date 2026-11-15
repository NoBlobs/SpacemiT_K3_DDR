# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
# Native unit tests (host gcc). Firmware headers are used with stub MMIO.
HOSTCC ?= gcc
GEN    ?= ../build/gen
PYTHON ?= python3 -I
OUT    := ../build/tests
CFLAGS := -std=gnu11 -Wall -Wextra -Werror -O1 -g -I. -I../src -I$(GEN)

TESTS := test_msgblk test_phypara test_phypara_fw test_devinit_calc

.PHONY: all clean
all: $(addprefix $(OUT)/,$(TESTS))
	@for t in $(TESTS); do $(OUT)/$$t || exit 1; done
	@echo "all native tests passed"

$(OUT)/phypara_order.c: ../docsClean/appendix-phypara-order.md ../tools/gen_phypara.py
	@mkdir -p $(OUT)
	$(PYTHON) ../tools/gen_phypara.py $< $@

$(OUT)/test_msgblk: test_msgblk.c
	@mkdir -p $(OUT)
	$(HOSTCC) $(CFLAGS) $< -o $@

$(OUT)/test_phypara: test_phypara.c $(OUT)/phypara_order.c
	$(HOSTCC) $(CFLAGS) $^ -o $@

$(OUT)/phypara_fw.c: ../docsClean/appendix-phypara-order.md ../tools/gen_phypara.py
	@mkdir -p $(OUT)
	$(PYTHON) ../tools/gen_phypara.py --fw $< $@

$(OUT)/test_phypara_fw: test_phypara_fw.c $(OUT)/phypara_fw.c $(OUT)/phypara_order.c
	$(HOSTCC) $(CFLAGS) $^ -o $@

$(OUT)/test_devinit_calc: test_devinit_calc.c ../src/train/devinit_calc.h
	@mkdir -p $(OUT)
	$(HOSTCC) $(CFLAGS) $< -o $@

clean:
	rm -rf ../build/tests
