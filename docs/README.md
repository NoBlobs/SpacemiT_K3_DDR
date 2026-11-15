<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# docs — project documentation

This folder holds the documentation of the free SpacemiT K3 LPDDR5 PMU firmware.
It is written by the clean team in reStructuredText (`.rst`) and built with
Sphinx. It complements, and never modifies, the clean-room specification in
[`../docsClean/`](../docsClean/README.md).

## Structure

```
docs/
├── index.rst            entry point and evidence labels
├── hardware/            the K3 DDR hardware as the firmware sees it
├── uboot-spl/           role of the vendor U-Boot SPL and our changes to it
├── firmware/            implementation of the free PMU firmware
├── conf.py              Sphinx configuration (Read the Docs theme)
├── Makefile             build entry points
└── requirements.txt     Python packages for the build (Sphinx, RTD theme, dark mode)
```

| Part | Contents |
|---|---|
| `hardware/` | System architecture, address views, DDRC/DPLL, PMU core (BCRs, RTC), message block, mailbox, PHY CSR map, results/persistence, bench access to the board. Restates and extends `docsClean/` with our measurements. |
| `uboot-spl/` | What the SPL does around the PMU run, the Kconfig options and code changes on the `k3-free-ddr-fw` branch of `u-boot_SpacemiT_K3`, the host mailbox receiver, how to build and stage `FSBL.bin`. |
| `firmware/` | Toolchain and flags, memory layout, runtime (startup, exceptions, timer), PMU mailbox, streaming IDs, DevInit, training engine and stages, quickboot, verification and the clean-room record. |

### Evidence labels

Every fact is tagged with where it comes from:

- `[spec]` — taken from `docsClean/`, not yet re-verified;
- `[exp NNN]` — established by experiment `../logsRefs/NNN-<name>/readme.md`;
- `[design]` — a decision of this project, not a hardware fact;
- `[open]` — unknown, still to be measured.

When an experiment contradicts `docsClean/`, this documentation says so and
cites the experiment (for example the PMU mailbox polarity, experiment 000).

### Licenses

Pages that adapt text from `docsClean/` (CC-BY-SA-4.0) carry `CC-BY-SA-4.0`,
as its ShareAlike term requires; the others carry `GPL-2.0-or-later`. Each
page states its SPDX identifier in its first line.

## Building

Requirements: `python3` with `venv` and network access on the first build.
Sphinx and the [Read the Docs theme](https://sphinx-rtd-theme.readthedocs.io/) (the theme of docs.openmc.org) with the
[dark mode toggle](https://github.com/MrDogeBro/sphinx_rtd_dark_mode) are installed
into `docs/.venv/` the first time.

```sh
cd docs
make            # HTML (default)  -> docs/_build/html/index.html
make html       # same
make serve      # build and serve on http://127.0.0.1:8765
make clean      # remove _build/
make distclean  # also remove .venv/
```

The pages open in the light theme; the round button at the bottom right
switches to dark mode and the browser remembers the choice (to start in dark
mode, set `default_dark_mode = True` in `conf.py`).

From the repository root, `make docs` builds the HTML through this Makefile.

`_build/` and `.venv/` are ignored by git (see `.gitignore`): generated output
is never committed.

## Writing

- One topic per page; add new pages to the `toctree` of their part's
  `index.rst`.
- Cite with the link roles defined in `conf.py`; they point to GitHub, so they
  work on the published site:

  | Role | Example | Links to |
  |---|---|---|
  | `:dc:` | ``:dc:`12` `` → docsClean/12 | `docsClean` chapter (or ``:dc:`appendix-phypara-order` ``), at the submodule commit pinned in this repository |
  | `:dc:` with title | ``:dc:`docsClean BR-09 <13>` `` | same, custom text |
  | `:lr:` | ``:lr:`001-probe-core-mailbox-rtc/mbox.txt` `` | file or folder under `logsRefs/` on `main` |
  | `:exp:` | ``[:exp:`001`]`` → [exp 001] | `logsRefs/001-*/readme.md` (warns if the experiment does not exist) |

- Start each new page with its SPDX line (`.. SPDX-License-Identifier: …`).

## Publishing (GitHub Pages)

`.github/workflows/docs.yml` builds the HTML with warnings treated as errors on
every push to `main` that touches `docs/` or `logsRefs/`, and deploys it with
GitHub Pages. Pull requests only build (check) without deploying.

One-time setup in the repository: **Settings → Pages → Build and deployment →
Source: GitHub Actions**. The site is then published at
<https://noblobs.github.io/SpacemiT_K3_DDR/>.

Links to `logsRefs/` point to the `main` branch, so they only resolve after
that content is pushed.
