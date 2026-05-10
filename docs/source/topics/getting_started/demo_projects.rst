Demo Projects
=============

A separate companion repository — `aetherion-demo-projects
<https://github.com/arthurmoreno/aetherion-demo-projects>`_ — hosts small,
runnable example games that exercise different parts of the engine. It is
the recommended starting point if you want to *use* Aetherion to build
something rather than work on the engine itself.

The repository is laid out as one folder per demo. Most are scaffolds that
illustrate intended structure for a given multiplayer / gameplay style, and
one is a fully implemented, runnable game.

Available Demos
---------------

* `01_persistent_world_mmo
  <https://github.com/arthurmoreno/aetherion-demo-projects/tree/main/01_persistent_world_mmo>`_
  — scaffolding for a client/server persistent-world demo. *Work in
  progress.*
* `02_coop_sandbox
  <https://github.com/arthurmoreno/aetherion-demo-projects/tree/main/02_coop_sandbox>`_
  — cooperative sandbox demo layout and example assets. *Work in
  progress.*
* `03_rogue_dungeon/minimal_dungeon
  <https://github.com/arthurmoreno/aetherion-demo-projects/tree/main/03_rogue_dungeon/minimal_dungeon>`_
  — a compact roguelike demo. **Fully implemented and runnable** — start
  here.
* `04_competitive_arena
  <https://github.com/arthurmoreno/aetherion-demo-projects/tree/main/04_competitive_arena>`_
  — competitive / match-networking demo scaffold. *Work in progress.*

The "Minimal Dungeon" demo showcases Aetherion's core rendering, input,
world and entity systems: it defines a simple world, a player entity, item
configuration, and SDL2-based rendering and input.

.. _running_minimal_dungeon:

Tutorial: Running the Minimal Dungeon Demo
------------------------------------------

This walkthrough takes the ``03_rogue_dungeon/minimal_dungeon`` demo from a
fresh clone to a running game window.

1. Install Aetherion
~~~~~~~~~~~~~~~~~~~~

The demos depend on the ``aetherion`` Python package being importable. If
you have not installed Aetherion yet, follow the engine
:ref:`installation` guide first.

A quick sanity check:

.. code-block:: bash

   python -c "import aetherion; print(aetherion.__file__)"

2. Clone the demo repository
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   git clone https://github.com/arthurmoreno/aetherion-demo-projects.git
   cd aetherion-demo-projects/03_rogue_dungeon/minimal_dungeon

3. Create the demo's virtual environment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The demo uses `uv <https://github.com/astral-sh/uv>`_ for environment and
dependency management. From inside the
``03_rogue_dungeon/minimal_dungeon`` folder:

.. code-block:: bash

   uv venv
   uv sync

This creates a ``.venv`` with the per-demo dependencies pinned in
``pyproject.toml`` / ``uv.lock``. ``aetherion`` itself must already be
installed (step 1) — the demo's lockfile pulls in everything else.

4. Run the game
~~~~~~~~~~~~~~~

.. code-block:: bash

   python run.py

You should get an SDL2 window with the dungeon scene running. The entry
point is `run.py
<https://github.com/arthurmoreno/aetherion-demo-projects/blob/main/03_rogue_dungeon/minimal_dungeon/run.py>`_;
it wires up the player view, item configuration, scene, and input map and
hands them to ``aetherion.engine.game.GameEngine``.

5. (Optional) Build a standalone executable
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The demo ships a PyInstaller spec for bundling a single-file executable:

.. code-block:: bash

   pyinstaller minimal_dungeon_game.spec

The resulting binary lives under ``dist/``.

Project Layout
~~~~~~~~~~~~~~

Inside ``03_rogue_dungeon/minimal_dungeon`` the relevant pieces are:

* ``run.py`` — entry point; constructs the ``GameEngine`` and starts the
  loop.
* ``settings.py`` — screen dimensions, FPS target, sprite scale.
* ``world/`` — world factory and terrain views.
* ``entities/`` — beast / item / player definitions and view classes.
* ``events/`` — game-event handlers (e.g. beast creation / connection).
* ``scenes/`` — scene composition (``MinimalScene``).
* ``resources/`` — sprite sets and per-style render assets (e.g.
  ``resources/dimetric``).
* ``assets/`` / ``data/`` — game assets and saved state.

For deeper run/build instructions per demo, consult the
demo-specific README inside each folder, e.g. `minimal_dungeon/README.md
<https://github.com/arthurmoreno/aetherion-demo-projects/blob/main/03_rogue_dungeon/minimal_dungeon/README.md>`_.
