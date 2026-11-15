<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Thalles Campagnani -->

# SpacemiT K3 — Free LPDDR5 PMU Firmware

Free (GPL-2.0-or-later, GNU C11) LPDDR5 training and quickboot firmware for the
embedded **ARCv2/EM** microcontroller (**PMU**) inside the DDR PHY of the
**SpacemiT K3** RISC-V SoC.

During early boot the vendor U-Boot SPL configures the two DDR controllers
(**DDRC0** at `0xcb000000`, **DDRC1** at `0xcc000000`) and their clocks, loads a
firmware image into the PHY PMU instruction memory (**IMEM**, host CSR index
`0x50000`), fills a 320-byte **message block** in data memory (**DMEM**, host CSR
index `0x58000`) and releases the PMU (`MICRO_RESET = 9 -> 1 -> 0`). Until now the
PMU programs shipped for the K3 were **proprietary binary blobs**. This project
replaces them with free software written from a clean-room specification.

## Clean-room process

1. **Dirty team (specification).** Hardware and interface facts are published in
   the read-only submodule [docsClean/](docsClean/README.md). It describes
   addresses, access widths, message layouts, mailbox protocol, ordering rules and
   known gaps — never vendor code.
2. **Clean team (implementation).** Everything outside `docsClean/` is written only
   from:
   - [docsClean/](docsClean/README.md);
   - public JEDEC LPDDR5 specifications (JESD209-5) and DRAM datasheets;
   - public **ARCv2/EM** ISA and GNU toolchain documentation;
   - public U-Boot SPL sources and device trees for the K3 (excluding the arrays
     that embed the proprietary PMU firmware);
   - controlled experiments on the reference board using **this** firmware.

The clean team never reads, runs on purpose, or disassembles the proprietary PMU
firmware. New findings go to [docs/](docs/index.rst) (project documentation) with the
raw evidence in [logsRefs/](logsRefs/README.md); `docsClean/` is never modified.

```
docsClean/ (read-only spec) ──> src/ (free PMU firmware) ──> U-Boot SPL (loads it)
                                      │
            docs/ (rst) <── logsRefs/NNN-experiment/ <── bench experiments
```

## Status and main risk

The specification fully covers the host/PMU interface (memory map, mailbox,
message block, results, ARC ABI) but **not** the bit-level meaning of most internal
PHY CSRs nor the reference training algorithms (`docsClean/15-open-questions.md`).
Full training therefore advances together with an experimental campaign on the
board; every experiment is recorded under `logsRefs/`.

## Repository layout

```
docsClean/   clean-room specification (git submodule, read-only, CC-BY-SA-4.0)
src/         ARCv2/EM PMU firmware (GNU C11 + ARC assembly)
  arch/      startup (reset @0x0 -> 0x1fc), linker script, libcall overrides
  hal/       16-bit MMIO, RTC timer, mailbox, streaming
  common/    message block layout, PHY CSR addressing
  probe/     M1 probe firmware (ISA/BCR, RTC, PHY signature)
  train/     full-training firmware
tools/       generators, image packer, ELF audit, bench helpers
tests/       native (host gcc) unit tests
docs/        project documentation (reStructuredText): hardware, U-Boot SPL, firmware
logsRefs/    reference logs of experiments, one directory per experiment (NNN-name)
```

## Building

Requires an ARCv2 EM bare-metal GCC. The Synopsys prebuilt
`arc_gnu_2026.03_prebuilt_elf32_le_linux_install` is expected extracted in
`tools/toolchain/` (git-ignored); see `docs/firmware/build-toolchain.rst`.

```sh
make                         # firmware (auto-detects the cross prefix)
make CROSS_COMPILE=arc-elf32-
make check                   # ELF audit: r16-r25, libgcc helpers, entry, layout, size
make test                    # native unit tests
make fsbl                    # build the vendor U-Boot FSBL with this firmware
```

U-Boot tree used: `https://github.com/NoBlobs/u-boot_SpacemiT_K3`. See [docs/uboot-spl/](docs/uboot-spl/index.rst).

## Key ARCv2/EM constraints

See `docsClean/12-arc-execution-environment.md` and
`docsClean/13-firmware-behavioral-requirements.md`:

1. `-mcpu=em -mrf16 -mmpy-option=wlh5 -mdiv-rem -mno-sdata` on every compile **and**
   on the link; `r16`–`r25` do not exist.
2. Reset at `0x0` branches to startup at `0x1fc`; stack top `0x80010000`; `.bss` at
   `0x80004000`; never clear `0x80000000`–`0x800009a3` (host input).
3. DMEM (`0x8000xxxx`) and PHY/mailbox CSRs (`0x90xxxxxx`) only through `volatile`
   16-bit (or 8-bit for byte fields) accesses.
4. RTC through AUX `0x103`–`0x105` with `lr`/`sr`; `__muldi3`, `__udivsi3`,
   `__divsi3` overridden with hardware instructions (built without LTO).

> **Hardware safety:** never access DPHY/PMU MMIO from Linux (`/dev/mem`); it
> stalls the SoC and needs a power cycle.

## License

Copyright (C) 2026 Thalles Campagnani

Firmware, tools and tests are free software under the GNU General Public License,
version 2 or (at your option) any later version. See [LICENSE](LICENSE).

`docsClean/` is licensed CC-BY-SA-4.0 by its authors. Pages of `docs/` that adapt
text from `docsClean/` carry `CC-BY-SA-4.0` as required by its ShareAlike term;
pages written from scratch carry `GPL-2.0-or-later`. Each file states its SPDX
identifier.

This project is not affiliated with or endorsed by SpacemiT, Synopsys, or Micron.
Programming DRAM controllers and PHYs is hardware-specific — use at your own risk.
