Profile Builds
==============

External profilers, sanitizers, and live tracers all need different
compiler flags to be useful. Aetherion exposes those as one-shot
build switches on the top-level ``make`` target — flip the switch,
rebuild, attach the tool. Each switch is a thin wrapper around a CMake
option, so downstream games consuming Aetherion as a CMake dependency
can flip the same flags directly.

The general shape of every recipe in this section is:

.. code-block:: bash

   cd aetherion-demo-projects/03_rogue_dungeon/minimal_dungeon
   make build <FLAG>=1 && make install
   # ... attach the tool of choice and run python run.py

``PROFILE=1`` — debug-friendly symbols for external profilers
-------------------------------------------------------------

The shipped switch. Use it any time you plan to attach valgrind,
heaptrack, or perf. Default release builds (``-O3 -flto``) collapse
inlinable functions into their callers and rip out frame pointers,
which leaves external profilers showing call stacks like
``[unknown] → [unknown] → main``. ``PROFILE=1`` keeps the binary fast
enough to reproduce the symptom while preserving the symbols those
tools need.

What it sets:

- ``-O1`` — most optimisations off so call stacks survive
- ``-g`` — full DWARF debug info
- ``-fno-omit-frame-pointer`` — frame pointer chain for sampling profilers
- ``-fno-inline`` — every function is its own frame
- ``-march=native -ffast-math`` — preserved so the perf profile resembles release

Build mode is forced to ``RelWithDebInfo`` so scikit-build-core does
not silently re-append ``-O3 -DNDEBUG -Wl,-s`` from its ``Release``
defaults.

Canonical invocation:

.. code-block:: bash

   cd aetherion-demo-projects/03_rogue_dungeon/minimal_dungeon
   make build PROFILE=1 && make install
   # ... ~3x slower at runtime than the default build; revert with PROFILE=0

Composes with every external-profiler recipe in :doc:`external_profilers`.

``SANITIZE=address`` — at-exit leak report (planned)
----------------------------------------------------

.. note::

   This build switch is **planned, not yet shipped**. The interface
   documented below is the intended shape — once the switch lands,
   this section will become operational guidance with a real
   LeakSanitizer report.

Once available, this is the right first stop for "RSS climbed during
play, what allocated it?" AddressSanitizer + LeakSanitizer instruments
every allocation with a shadow record; LSan walks the live set at
process exit and dumps every unreachable allocation with the full
call stack of the original ``malloc``/``new``. Compared to valgrind,
the runtime overhead is far lower (~2x vs ~20x) and the output is
more direct — you get a stack trace per leak instead of a tree of
samples to diff.

Intended invocation:

.. code-block:: bash

   cd aetherion-demo-projects/03_rogue_dungeon/minimal_dungeon
   make build SANITIZE=address && make install
   ASAN_OPTIONS="detect_leaks=1" python run.py
   # ... play long enough to reproduce the leak, then Ctrl-C
   # ... LSan dumps every leaked allocation with its allocation stack

``TRACY=1`` — in-process live profiler (planned)
------------------------------------------------

.. note::

   This build switch is **planned, not yet shipped**. See
   :doc:`tracy_profiler` for the design summary; this page will
   document the operational details once the integration ships.

When live: links the engine against the Tracy client, exposing
per-frame timing zones and allocation events to a UDP-attached Tracy
viewer. Right tool for "frame rate decays over a long session" or
"I want to watch RSS climb in real time without waiting for an
at-exit dump."

Composability
-------------

The flags compose with ``+=`` semantics — ``PROFILE=1 SANITIZE=address``
is valid for the rare full-fat investigation. In practice, run them
one at a time: each one slows the binary, and the diagnoses they
produce point at different layers (call stacks, allocation
attribution, live timeline). Composing them adds noise more often
than it adds signal.
