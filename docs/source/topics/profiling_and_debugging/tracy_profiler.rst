Tracy Profiler
==============

`Tracy <https://github.com/wolfpld/tracy>`_ is a low-overhead, in-process
frame and memory profiler. Aetherion ships a vendored Tracy client
(``libs/tracy``) that links into the engine when you build with
``TRACY=1``, plus the Tracy viewer (``tracy-profiler``) which connects
over UDP and renders a live timeline.

It is the right tool for:

- Watching RSS climb in real time during a long session, instead of
  waiting for an at-exit dump from LeakSanitizer or valgrind.
- Visualising per-frame timing zones as a flame graph, to spot a
  frame-rate decay before it becomes a hitch.
- Attributing allocations to specific tick subsystems on the live
  timeline, paired with the ``aetherion::diag`` counters that hint
  at *where* to zoom in (see :doc:`diagnostic_module`).

Default ``make build`` produces a Tracy-free binary — every
instrumentation macro is dead-code-eliminated because ``TRACY_ENABLE``
is undefined. You only pay the overhead when you opt in.

Build the engine with Tracy
---------------------------

The build switch is ``TRACY=1`` (or the env-var alias
``AETHERION_TRACY=1``):

.. code-block:: bash

   make build TRACY=1
   make install

Internally that flips ``-DAETHERION_TRACY_BUILD=ON`` and pairs it with
``-Ccmake.build-type=RelWithDebInfo`` so scikit-build-core's
Release-only auto-strip does not erase the DWARF Tracy needs to
symbolicate stacks. The CMake block also clears the per-config flag
defaults so the global ``-O3 -march=native -flto`` are not silently
downshifted to ``-O2``.

What the switch sets:

- ``TRACY_ENABLE`` defined project-wide — every ``ZoneScoped`` /
  ``FrameMark`` / ``TracyAllocS`` macro becomes live.
- ``TRACY_CALLSTACK=15`` — depth of the libunwind call-stack capture
  per allocation. Required for the Memory tab's bottom-up and
  top-down call trees to populate.
- ``TRACY_ON_DEMAND`` — the profiler stays idle until a Tracy viewer
  connects. Runtime overhead is sub-1 % when no viewer is attached;
  ~5 % during live capture. Set ``TRACY_ON_DEMAND=OFF`` in CMake for
  always-on capture (CI capture scenarios).
- ``TRACY_STATIC`` — Tracy is static-linked into ``_aetherion.so``;
  no extra shared objects to ship.

``make build TRACY=1`` is independent from ``PROFILE=1`` (see
:doc:`profile_builds`); the two flags compose, but in practice run
them one at a time.

Build the Tracy viewer
----------------------

The viewer source lives under ``libs/tracy/profiler``. A standard
CMake build produces the ``tracy-profiler`` binary:

.. code-block:: bash

   cd libs/tracy/profiler/build
   cmake ..
   make -j$(nproc)
   ls tracy-profiler   # → ./tracy-profiler

Add it to your ``$PATH`` if you want to launch it from anywhere.

The viewer needs system OpenGL / Wayland / X11 dev libraries plus
``libcapstone`` and ``libfreetype`` — Tracy's CMake script will tell
you which packages are missing. On Ubuntu most distributions ship them
under ``libgl1-mesa-dev``, ``libwayland-dev``, ``libxkbcommon-dev``,
``libcapstone-dev``, ``libfreetype-dev``.

Tutorial: Live frame + memory capture
-------------------------------------

This walks from a fresh shell to a connected Tracy session.

1. Build with Tracy and reinstall the wheel
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   make build TRACY=1
   make install

Verify the resulting binary is Tracy-instrumented:

.. code-block:: python

   python -c "import aetherion; print(aetherion.tracy_enabled())"
   # → True

2. Launch the viewer
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   ./libs/tracy/profiler/build/tracy-profiler &

The viewer opens to a connection picker. With ``TRACY_ON_DEMAND``,
the engine does *not* announce itself until a viewer is listening —
launching the viewer first is the more reliable order.

3. Run a demo and connect
~~~~~~~~~~~~~~~~~~~~~~~~~

In a second terminal, start any demo (e.g. the
:ref:`running_minimal_dungeon` walkthrough):

.. code-block:: bash

   cd aetherion-demo-projects/03_rogue_dungeon/minimal_dungeon
   python run.py

The Tracy viewer should auto-discover the running engine over UDP
broadcast on the local machine. If it does not (e.g. on a remote
host or a sandboxed container), enter the host manually in the
viewer's "Connect to client" dialog — Tracy listens on TCP
``8086`` by default.

Once connected, the timeline starts populating immediately and the
Memory tab begins recording every allocation.

4. Disconnect and reconnect cleanly
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Closing the viewer drops the engine back to idle (sub-1 % overhead).
Re-opening the viewer reconnects without restarting the demo. This is
the right pattern for "let it run for an hour, then take a look" —
keep the engine running, attach the viewer when you want to inspect.

Built-in zones
--------------

The engine instruments its tick orchestrator and the heaviest passes
with named ``ZoneScopedN`` markers. They appear as labelled blocks on
the per-thread timeline:

- ``World::update`` (:file:`src/World.cpp`) — top-level frame
- ``PhysicsEngine::processPhysics`` /
  ``PhysicsEngine::processPhysicsAsync`` (:file:`src/PhysicsEngine.cpp`)
- ``EcosystemEngine::processEcosystem`` /
  ``EcosystemEngine::processEcosystemAsync`` (:file:`src/EcosystemEngine.cpp`)

A ``FrameMark`` is emitted at each ``World::update`` boundary, which
is what populates the Frames panel and the per-frame timing
histogram.

To add your own zone in C++:

.. code-block:: cpp

   #ifdef TRACY_ENABLE
   #include <tracy/Tracy.hpp>
   #endif

   void MySystem::processFoo() {
   #ifdef TRACY_ENABLE
     ZoneScopedN("MySystem::processFoo");
   #endif
     // ... work ...
   }

The ``#ifdef`` guard is important: it keeps the include and the macro
out of non-Tracy builds (where ``ZoneScopedN`` would still expand to
``((void)0)`` but the include path would still be touched).

Python-side instrumentation
---------------------------

Three helpers are exposed unconditionally on the ``aetherion`` module
so Python callers don't need to special-case the Tracy build:

.. code-block:: python

   import aetherion

   if aetherion.tracy_enabled():
       aetherion.tracy_message("scenario: heavy-rain start")

   for tick in range(N):
       world.update()
       aetherion.tracy_frame_mark()  # custom frame boundary if you
                                     # don't go through World::update

When the engine is built without Tracy, all three become no-ops:

- ``tracy_enabled()`` returns ``False``
- ``tracy_frame_mark()`` does nothing
- ``tracy_message(msg)`` discards the message

Use ``tracy_message`` to label scenario phases on the timeline (e.g.
"populate world", "drought scenario", "tear down") — it's the
fastest way to find a region of interest in a long capture.

Finding a memory leak
---------------------

Tracy's Memory tab is the entry point for "RSS keeps climbing — what
is allocating?".

1. **Open the Memory tab.** It shows total live bytes, allocation
   count, and a histogram of sizes over time.

2. **Watch the live-bytes trace.** A leak shows up as a monotonically
   climbing line; a normal subsystem warming up shows up as a step
   followed by a plateau.

3. **Switch to the bottom-up call tree.** This aggregates every
   currently-live allocation by its top-of-stack frame. The function
   that allocated the largest still-resident bytes is at the top.
   This view only populates because the build defines
   ``TRACY_CALLSTACK=15`` — without per-allocation stacks, the call
   tree is blank.

4. **Switch to the top-down call tree** to confirm the path from
   ``main`` / ``World::update`` down into the suspect allocator.
   Cross-reference with ``aetherion::diag`` per-subsystem byte
   gauges (see :doc:`diagnostic_module`) to confirm the same
   subsystem.

The new/delete overrides that feed this view live in
:file:`src/diag/TracyMemory.cpp`. They route every C++ allocation
through ``TracyAllocS`` / ``TracyFreeS`` (the ``S`` variants — the
ones that capture call stacks). The whole TU is gated on
``TRACY_ENABLE``, so the default build pays nothing.

Allocations made by Python objects, OpenVDB pools, or third-party
libraries that do their own ``mmap`` / ``brk`` will *not* show up in
the Memory tab — those bypass ``operator new``. For mmap-level
attribution use ``valgrind --tool=massif --pages-as-heap=yes`` (see
:doc:`external_profilers`).

Finding a slow function
-----------------------

Tracy's frame timeline is the entry point for "frame rate decays over
a long session".

1. **Open the Frames panel.** Each ``FrameMark`` from
   ``World::update`` is one bar. The panel highlights frames that
   exceed the median + a configurable threshold.

2. **Click a slow frame** to scrub the timeline to that tick. The
   per-thread track shows every ``ZoneScopedN`` block that ran, with
   exact start/end times and self-time.

3. **Hover a zone** to see its source location, parent zone, and
   exact duration.

4. **Right-click a zone → "Statistics"** to see the distribution of
   that zone's duration across the whole capture — the right view
   for "this function is sometimes slow but I can't tell when".

Pair with ``perf record`` (:doc:`external_profilers`) once you've
narrowed by zone: Tracy shows you *which* zone regressed, perf shows
you *which instructions inside it* are hot.

Composing with diag counters
----------------------------

The durable instrumentation in :doc:`diagnostic_module` and Tracy
serve different jobs:

- ``aetherion::diag`` counters / gauges flush to ``GameDB`` and
  survive across runs. Use them to *detect* that something
  regressed between yesterday's run and today's.
- Tracy is a live, in-the-moment view. Use it to *attribute* the
  regression to a call stack or a frame.

A typical workflow: a diag gauge shows ``rss_mb`` climbing in a
nightly run → reproduce locally with ``make build TRACY=1`` →
attach the viewer → walk the bottom-up call tree until the leaking
allocator is named.
