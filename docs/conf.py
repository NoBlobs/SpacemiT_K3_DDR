# SPDX-License-Identifier: GPL-2.0-or-later
# Sphinx configuration. Build the HTML with `make` in this folder.
import re
import subprocess
from pathlib import Path

from docutils import nodes

project = "SpacemiT K3 free DDR PMU firmware"
author = "Thalles Campagnani"
copyright = "2026, Thalles Campagnani"
language = "en"

# sphinx_rtd_dark_mode adds a light/dark toggle button to the RTD theme;
# the choice is remembered by the browser.
extensions = ["sphinx_rtd_theme", "sphinx_rtd_dark_mode"]
default_dark_mode = False
exclude_patterns = ["_build", ".venv", "README.md", "questions.md", "requirements.txt"]

# Read the Docs theme (same as docs.openmc.org)
html_theme = "sphinx_rtd_theme"
html_title = "SpacemiT K3 — free DDR PMU firmware"
html_show_sourcelink = False
html_theme_options = {
    "navigation_depth": 3,
    "collapse_navigation": False,
    "sticky_navigation": True,
    "prev_next_buttons_location": "both",
}
pygments_style = "sphinx"

# _static/tables.css: let table cells wrap (the theme keeps them on one line).
# _static/theme_persist.js: keep the light/dark choice across local files.
html_static_path = ["_static"]
html_css_files = ["tables.css"]
html_js_files = ["theme_persist.js"]

# ---------------------------------------------------------------------------
# Links to the repository and to the docsClean submodule on GitHub.
#
#   :dc:`12`                         docsClean/12 -> chapter 12 file
#   :dc:`appendix-phypara-order`     appendix file
#   :dc:`docsClean/ <README>`        explicit title
#   :lr:`001-probe-core-mailbox-rtc/mbox.txt`  file or directory in logsRefs/
#   :exp:`001`                       "exp 001" -> logsRefs/001-*/readme.md
# ---------------------------------------------------------------------------
REPO_URL = "https://github.com/NoBlobs/SpacemiT_K3_DDR"
REPO_BRANCH = "main"
DOCSCLEAN_URL = "https://github.com/NoBlobs-docsClean/SpacemiT_K3_DDR"

ROOT = Path(__file__).resolve().parent.parent

DOCSCLEAN_FILES = {
    "01": "01-system-architecture.md",
    "02": "02-addressing-and-access.md",
    "03": "03-ddrc-and-clock.md",
    "04": "04-parts-topologies-and-timings.md",
    "05": "05-message-block.md",
    "06": "06-full-initialization.md",
    "07": "07-training-behavior.md",
    "08": "08-mailbox-and-streaming.md",
    "09": "09-training-results.md",
    "10": "10-persistence-and-selection.md",
    "11": "11-quickboot.md",
    "12": "12-arc-execution-environment.md",
    "13": "13-firmware-behavioral-requirements.md",
    "14": "14-validation-and-current-status.md",
    "15": "15-open-questions.md",
    "16": "16-flash-build-and-recovery.md",
}


def _docsclean_commit():
    """Commit of docsClean pinned by this repository (gitlink), so links show
    exactly the revision the documentation was written against."""
    try:
        # index entry "160000 <sha> 0\tdocsClean" (works before and after commit)
        out = subprocess.run(["git", "-C", str(ROOT), "ls-files", "-s", "docsClean"],
                             capture_output=True, text=True, check=True)
        fields = out.stdout.split()
        if len(fields) >= 2 and fields[0] == "160000":
            return fields[1]
    except (OSError, subprocess.CalledProcessError):
        pass
    return "main"


DOCSCLEAN_REV = _docsclean_commit()
_TITLED = re.compile(r"^(.*?)\s*<(.*)>$", re.S)


def _split(text):
    m = _TITLED.match(text)
    return (m.group(1), m.group(2)) if m else (None, text)


def _link(rawtext, title, url):
    return [nodes.reference(rawtext, title, refuri=url)], []


def dc_role(name, rawtext, text, lineno, inliner, options=None, content=None):
    title, target = _split(text)
    target = target.strip()
    if target in ("", "/", "README"):
        url = f"{DOCSCLEAN_URL}/tree/{DOCSCLEAN_REV}"
        return _link(rawtext, title or "docsClean/", url)
    fname = DOCSCLEAN_FILES.get(target.zfill(2) if target.isdigit() else target)
    if fname is None:
        fname = target if target.endswith(".md") else target + ".md"
    url = f"{DOCSCLEAN_URL}/blob/{DOCSCLEAN_REV}/{fname}"
    return _link(rawtext, title or f"docsClean/{target}", url)


def lr_role(name, rawtext, text, lineno, inliner, options=None, content=None):
    title, target = _split(text)
    target = target.strip().strip("/")
    kind = "blob" if "." in target.rsplit("/", 1)[-1] else "tree"
    url = f"{REPO_URL}/{kind}/{REPO_BRANCH}/logsRefs/{target}"
    return _link(rawtext, title or f"logsRefs/{target}", url)


def exp_role(name, rawtext, text, lineno, inliner, options=None, content=None):
    title, num = _split(text)
    num = num.strip()
    matches = sorted((ROOT / "logsRefs").glob(f"{num}-*"))
    if not matches:
        msg = inliner.reporter.warning(f"no logsRefs/{num}-* experiment", line=lineno)
        return [nodes.literal(rawtext, f"exp {num}")], [msg]
    url = f"{REPO_URL}/blob/{REPO_BRANCH}/logsRefs/{matches[0].name}/readme.md"
    return _link(rawtext, title or f"exp {num}", url)


def setup(app):
    app.add_role("dc", dc_role)
    app.add_role("lr", lr_role)
    app.add_role("exp", exp_role)
    return {"parallel_read_safe": True}
