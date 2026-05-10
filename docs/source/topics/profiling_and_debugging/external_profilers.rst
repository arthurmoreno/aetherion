External Profilers
==================

External tools answer the questions ``aetherion::diag`` cannot:
"which exact call stack allocated those bytes," "where is the
process spending CPU between two diag samples." Each tool has a
sweet spot and a failure mode. Pick by symptom, not by familiarity.

Every recipe in this section assumes a ``PROFILE=1`` build (see
:doc:`profile_builds`). Without it, optimised inlining will collapse
the very call stacks you came to inspect.

valgrind ``--tool=massif``
--------------------------

Heap profiler. Walks the process at sampled checkpoints, records
which call stack is responsible for each live allocation, and emits
a tree-shaped report. The right tool when the question is
"what is *cumulatively* alive in heap, attributed to call stacks?"

Two modes that answer different questions.

``--pages-as-heap=no`` (default)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Tracks every ``malloc``/``new``/``mmap``-via-glibc-malloc call. Fast,
low overhead, but blind to allocations served from arenas the
allocator already owns — a per-tick growing ``std::map`` inside a
pre-warmed arena will look invisible. Use for clearly-allocator-driven
leaks.

.. code-block:: bash

   cd aetherion-demo-projects/03_rogue_dungeon/minimal_dungeon
   make build PROFILE=1 && make install
   valgrind --tool=massif \
            --time-unit=B \
            --detailed-freq=10 --threshold=0.5 \
            --massif-out-file=/tmp/massif.minimal_dungeon.out \
            python run.py
   # ... reproduce the symptom, then Ctrl-C
   ms_print /tmp/massif.minimal_dungeon.out > /tmp/massif.txt

``--pages-as-heap=yes``
~~~~~~~~~~~~~~~~~~~~~~~

Tracks every page-level mapping (raw ``mmap``, including allocator
arena growth and library-internal pools). Catches what the default
mode misses — but also flags one-shot startup mappings from CUDA,
OpenBLAS, or any library that mmaps a large workspace. Use as the
fallback when default mode shows a flat profile but RSS is climbing.

.. code-block:: bash

   valgrind --tool=massif \
            --time-unit=B \
            --pages-as-heap=yes \
            --detailed-freq=10 --threshold=0.5 \
            --massif-out-file=/tmp/massif-pages.out \
            python run.py

.. warning::

   ``-O3 -flto`` collapses call stacks. Massif will happily report
   leaks against ``[unknown] → [unknown] → main`` if you forgot to
   build with ``PROFILE=1``. If your output looks like that, rebuild
   and re-run before reading further.

.. warning::

   Multiprocessing children spawn separate massif files with PID
   suffixes (``massif.minimal_dungeon.out.<pid>``). Analyse the
   parent file unless the leak is in a worker — the parent file
   covers the main interpreter, which is where most game-logic
   allocation lives.

heaptrack
---------

Same shape as massif, faster, with a far better UI (a Qt viewer that
visualises the allocation timeline and lets you bisect by stack).
The right tool when massif reports something interesting and you
want to drill into the timeline interactively.

.. code-block:: bash

   cd aetherion-demo-projects/03_rogue_dungeon/minimal_dungeon
   make build PROFILE=1 && make install
   heaptrack python run.py
   # ... reproduce, then Ctrl-C
   # ... outputs heaptrack.python.<pid>.zst; open with `heaptrack_gui`

.. warning::

   **Conda env trap.** A system-installed ``heaptrack`` binary links
   against the system ``libstdc++``, which may not match the
   ``libstdc++`` shipped inside an Aetherion conda env. The mismatch
   manifests as a segfault before ``main()`` runs.

   Workaround: build heaptrack from source against the conda env's
   stdlib. From inside the conda env::

       git clone https://github.com/KDE/heaptrack
       cd heaptrack && mkdir build && cd build
       cmake -DCMAKE_INSTALL_PREFIX="$CONDA_PREFIX" ..
       make -j && make install

   The resulting ``heaptrack`` binary picks up the right
   ``libstdc++`` automatically when launched from the same env.

AddressSanitizer / LeakSanitizer
--------------------------------

Compiled-in instrumentation, not a runtime attach. Lower overhead
than valgrind (~2x vs ~20x), produces a definitive at-exit leak
report with full allocation stacks. Right first stop for
"definitely a leak somewhere, want the stack." See
:doc:`profile_builds` for the ``SANITIZE=address`` build switch.

perf
----

CPU profiler, not a memory tool. Right tool when the symptom is
"frame rate dropped" with no obvious memory growth. Quick recipe
for the impatient:

.. code-block:: bash

   cd aetherion-demo-projects/03_rogue_dungeon/minimal_dungeon
   make build PROFILE=1 && make install
   perf record -F 99 -g -- python run.py
   # ... reproduce, Ctrl-C
   perf report

For deeper analysis (flamegraphs, off-CPU profiling, scheduler
tracing), defer to the standard Linux ``perf``
documentation — every recipe there works against a ``PROFILE=1`` build.
