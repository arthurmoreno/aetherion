# Aetherion Docs — Claude Code Context

Sphinx documentation site for Aetherion, integrating C++ API reference via Doxygen + Breathe.

## Directory Layout

```
docs/
  source/          — Sphinx source (conf.py, index.rst, topics/)
  build/           — Generated HTML output (sphinx-html target)
  doxygen/xml/     — Doxygen XML output consumed by Breathe
  requirements.txt — Python deps for the docs conda env
Doxyfile           — Doxygen config (project root, INPUT = src/, XML_OUTPUT = xml)
```

## Conda Environment

`aetherion-docs-312` — separate from the engine build env.

Install deps:
```sh
conda run -n aetherion-docs-312 pip install -r docs/requirements.txt
```

Current pinned deps (`requirements.txt`):
- `furo==2024.8.6`
- `Sphinx==8.1.3`
- `myst-parser==4.0.0`
- `breathe==4.36.0`

## Build Commands

| Task | Command | Run from |
|------|---------|----------|
| Generate C++ XML | `doxygen Doxyfile` | project root |
| Build HTML site | `make sphinx-html` | `docs/` |
| Build HTML (direct) | `sphinx-build -b html source build` | `docs/` |
| Generate Python RST stubs | `sphinx-apidoc -f -o . ../src` | `docs/source/` |

Full rebuild from scratch (both steps must run in order):
```sh
# From project root:
doxygen Doxyfile
cd docs && make sphinx-html
```

## Sphinx Extensions (conf.py)

| Extension | Purpose |
|-----------|---------|
| `breathe` | Renders Doxygen XML into Sphinx — C++ API reference |
| `myst_parser` | Allows `.md` files as Sphinx source pages |
| `sphinx.ext.autodoc` | Auto-generates docs from Python docstrings |
| `sphinx.ext.napoleon` | Google/NumPy docstring style support |
| `sphinx.ext.autosummary` | Summary tables for modules/classes |
| `sphinx.ext.viewcode` | Links to highlighted Python source |
| `sphinx.ext.graphviz` | Renders Graphviz diagrams (`graphviz` must be installed) |
| `sphinx.ext.coverage` | Reports docstring coverage |

Breathe config points to `../doxygen/xml` (relative to `source/conf.py`):
```python
breathe_projects = {"Aetherion": os.path.abspath("../doxygen/xml")}
breathe_default_project = "Aetherion"
```

## Theme

Current: **furo**

Previously explored: `sphinx-pdj-theme` (install: `pip install sphinx-pdj-theme`; activate by
uncommenting the `html_theme` lines in `conf.py`).

## Doxygen Notes

- Config: `Doxyfile` at project root
- Reads: `src/` (recursive)
- Outputs XML to: `xml/` at project root — Breathe expects it at `docs/doxygen/xml/`
  so confirm `OUTPUT_DIRECTORY` or copy/symlink as needed if the path drifts.
- Initialize a fresh Doxyfile: `doxygen -g`
