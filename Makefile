# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
#
# Free PMU firmware for the SpacemiT K3 DDR PHY (ARCv2 EM, RF16).
#
#   make [CROSS_COMPILE=arc-elf32-]   build all firmware images
#   make check                         audit ELF images (docsClean BR-10)
#   make test                          native unit tests
#   make fsbl [FW=probe|train]         build vendor U-Boot FSBL with that firmware

TOOLCHAIN_DIRS := $(wildcard $(CURDIR)/tools/toolchain/*/bin $(CURDIR)/tools/toolchain/*/*/bin \
	/opt/zephyr-sdk*/gnu/arc-zephyr-elf/bin \
	/opt/zephyr-sdk*/arc-zephyr-elf/bin /opt/arc*/bin)
ifeq ($(origin CROSS_COMPILE),undefined)
CROSS_COMPILE := $(firstword \
	$(foreach d,$(TOOLCHAIN_DIRS), \
		$(foreach p,arc-elf32- arc-snps-elf- arc-zephyr-elf-, \
			$(if $(wildcard $(d)/$(p)gcc),$(d)/$(p)))) \
	$(foreach p,arc-elf32- arc-snps-elf- arc-zephyr-elf- arc-linux-gnu-, \
		$(if $(shell command -v $(p)gcc 2>/dev/null),$(p))))
endif

CC      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
NM      := $(CROSS_COMPILE)nm
PYTHON  ?= python3 -I
HOSTCC  ?= gcc

UBOOT_DIR ?= /home/thalles/0-git/NoBlobs/u-boot_SpacemiT_K3
FW        ?= train

B := build

# docsClean/12, /13: RF16 + hardware mpy/div, no gp small data, on compile AND link.
# -mbarrel-shifter: BARREL_BUILD (AUX 0x7f) = 0x303 (logsRefs/001).
ARCH_FLAGS := -mcpu=em -mrf16 -mlittle-endian -mcode-density -mbarrel-shifter \
	-mmpy-option=wlh5 -mdiv-rem -mno-sdata
CFLAGS := -std=gnu11 $(ARCH_FLAGS) -Os -g -ffreestanding -fno-builtin \
	-fno-common -ffunction-sections -fdata-sections \
	-fno-tree-loop-distribute-patterns -fno-pic \
	-Wall -Wextra -Werror=implicit-function-declaration \
	-Isrc -I$(B)/gen
ASFLAGS := $(ARCH_FLAGS) -Isrc
LDFLAGS := $(ARCH_FLAGS) -nostdlib -nostartfiles -static \
	-Wl,--gc-sections -Wl,-T,src/arch/pmu.ld

MSGBLK_H := $(B)/gen/common/msgblk_fields.h

COMMON_SRC := src/arch/start.S src/hal/timer.c src/hal/mailbox.c \
	src/common/log.c src/common/exception.c
LIBCALLS := src/arch/libcalls.c

PROBE_SRC := src/probe/main.c
CSRDUMP_SRC := src/csrdump/main.c $(B)/gen/phypara_fw.c
CSRSCAN_SRC := src/csrscan/main.c
PLLPROBE_SRC := src/pllprobe/main.c
TRAIN_SRC := $(wildcard src/train/*.c)

FIRMWARES := probe csrdump csrscan pllprobe train

.PHONY: all check test fsbl docs clean
all: $(foreach f,$(FIRMWARES),$(B)/$(f)/$(f).imem.bin)

$(MSGBLK_H): docsClean/appendix-message-block-fields.md tools/gen_msgblk.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/gen_msgblk.py $< $@

NEED_CC = @test -n "$(CROSS_COMPILE)" || { echo "No ARCv2 toolchain found: install one (e.g. AUR zephyr-sdk-gnu-toolchain-arc-zephyr-elf-bin) or set CROSS_COMPILE=" >&2; exit 1; }

$(B)/obj/%.o: src/%.c $(MSGBLK_H)
	$(NEED_CC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(B)/gen/phypara_fw.c: docsClean/appendix-phypara-order.md tools/gen_phypara.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/gen_phypara.py --fw $< $@

$(B)/obj/gen/%.o: $(B)/gen/%.c
	$(NEED_CC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(B)/obj/%.o: src/%.S
	$(NEED_CC)
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

obj = $(patsubst $(B)/gen/%,$(B)/obj/gen/%.o,$(patsubst src/%,$(B)/obj/%.o,$(basename $(1))))

# libcalls.o is linked last and never with LTO (docsClean BR-07).
define FW_RULES
$(B)/$(1)/$(1).elf: $(call obj,$(COMMON_SRC) $(LIBCALLS) $(2)) src/arch/pmu.ld
	@mkdir -p $$(dir $$@)
	$$(CC) $$(LDFLAGS) -Wl,-Map,$(B)/$(1)/$(1).map -o $$@ \
		$(call obj,$(COMMON_SRC) $(2)) $(call obj,$(LIBCALLS)) -lgcc

$(B)/$(1)/$(1).imem.bin: $(B)/$(1)/$(1).elf tools/mkfw.py
	$$(PYTHON) tools/mkfw.py --objcopy $$(OBJCOPY) $$< $(B)/$(1)/$(1)
endef

$(eval $(call FW_RULES,probe,$(PROBE_SRC)))
$(eval $(call FW_RULES,csrdump,$(CSRDUMP_SRC)))
$(eval $(call FW_RULES,csrscan,$(CSRSCAN_SRC)))
$(eval $(call FW_RULES,pllprobe,$(PLLPROBE_SRC)))
$(eval $(call FW_RULES,train,$(TRAIN_SRC)))

check: all
	@for f in $(FIRMWARES); do \
		OBJDUMP=$(OBJDUMP) NM=$(NM) tools/audit.sh $(B)/$$f/$$f.elf || exit 1; \
	done

test: $(MSGBLK_H)
	@$(MAKE) -C tests HOSTCC=$(HOSTCC) GEN=$(abspath $(B)/gen)

# experiment firmwares (anything but train) stop SPL after the PMU run
FSBL_FRAG ?= $(if $(filter train,$(FW)),configs/k3_freefw.config,configs/k3_freefw_probe.config)

fsbl: $(B)/$(FW)/$(FW).imem.bin
	tools/build_fsbl.sh $(UBOOT_DIR) $(abspath $(B)/$(FW)/$(FW)) $(FSBL_FRAG)

# HTML documentation (only on request): see docs/README.md
docs:
	@$(MAKE) -C docs html

clean:
	rm -rf build
	@$(MAKE) -C tests clean

-include $(shell find $(B)/obj -name '*.d' 2>/dev/null)
