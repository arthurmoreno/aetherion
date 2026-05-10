Diagnostic Module (``aetherion::diag``)
=======================================

The ``aetherion::diag`` module is the engine's official, durable
in-process instrumentation API. Every other technique on this page
(profile builds, external profilers, case studies) composes with it —
counters and gauges identify *what* is growing or slowing, then
external tools pinpoint *where* in the call stack.

Why this module exists
----------------------

Without a single instrumentation API, every engine subsystem grows its
own ad-hoc metrics map and per-frame flush method. That sprawl rots
fast: producers register the same series under different names, sinks
fork into bespoke writers, and the cadence drifts subsystem to
subsystem. ``aetherion::diag`` collapses that into one Registry, one
flush rule, and a small handful of typed handles.

The four primitives
-------------------

:cpp:class:`aetherion::diag::Counter`
   Monotonically increasing 64-bit accumulator. Use for
   "how many times did *X* happen since startup" — bullets fired,
   entities spawned, messages dispatched. Aggregated by sum, flushed
   on a fixed cadence.

:cpp:class:`aetherion::diag::Gauge`
   Sampled scalar. Use for "what is the current value of *X*" — RSS,
   queue depth, terrain-grid byte count. Configurable aggregation
   (``Last``, ``Mean``, ``Min``, ``Max``, ``Sum``) collapses
   multiple samples within one flush window into a single point.

``Histogram``
   Latency-distribution primitive. *Planned (v2 of the diag module).*
   Use once shipped for "how long does this code path take per call"
   instead of stamping start/end pairs by hand.

:cpp:class:`aetherion::diag::EventLogger`
   Structured-record sink. Use for "when *X* happened, here are the
   five fields that describe it." Required-keys validation catches
   payload drift at the call site instead of at the analysis step.

All four handles are cheap shared pointers to PIMPL state.
Default-constructed handles are inert (``enabled() == false``), so
engines can hold them as members without two-phase init.

Producer-side: counter
----------------------

The canonical "I want to count something" pattern — declare once at
file scope, increment in the hot path:

.. code-block:: cpp

   #include "diag/Diag.hpp"
   using namespace aetherion::diag;

   namespace {
   auto bullets_fired = Registry::instance().counter({
       .name = "weapons.bullets_fired",
       .description = "Total bullets fired across all entities",
       .unit = "events",
       .flush_every = std::chrono::seconds{1},
       .sinks = {GameDBSink{}},
   });
   } // namespace

   void Weapon::fire() {
       bullets_fired.inc();
       // ... rest of fire logic
   }

The Registry coalesces the per-tick increments and writes one
time-series point per ``flush_every`` window — the hot path stays
allocation-free.

Producer-side: gauge with aggregation
-------------------------------------

For a sampled state, choose the aggregation that matches what you'll
plot. ``Last`` is right for slow-moving values (RSS); ``Max`` is
right for spike-detection (peak queue depth in the window):

.. code-block:: cpp

   #include "diag/Diag.hpp"
   using namespace aetherion::diag;

   namespace {
   auto entity_queue_depth = Registry::instance().gauge({
       .name = "ai.entity_queue_depth",
       .description = "Pending entities in the AI scheduler queue",
       .unit = "entries",
       .aggregation = AggFn::Max,
       .flush_every = std::chrono::seconds{1},
       .sinks = {GameDBSink{}},
   });
   } // namespace

   void AIScheduler::tick() {
       entity_queue_depth.set(static_cast<double>(m_pending.size()));
       // ... drain queue
   }

Producer-side: event logger
---------------------------

For one-off structured records — collisions, save events, cheat
detections — use ``EventLogger``. The ``required_keys`` list is a
contract: the call site must provide every named key, or the log call
asserts in debug and is dropped in release.

.. code-block:: cpp

   #include "diag/Diag.hpp"
   #include <nlohmann/json.hpp>
   using namespace aetherion::diag;

   namespace {
   auto save_events = Registry::instance().event({
       .channel = "persistence.save",
       .required_keys = {"slot", "duration_ms", "byte_size"},
       .sinks = {SpdlogSink{.logger_name = "diag_file"}},
   });
   } // namespace

   void SaveSystem::commit(int slot, int byte_size, std::chrono::milliseconds elapsed) {
       save_events.log({
           {"slot", slot},
           {"duration_ms", elapsed.count()},
           {"byte_size", byte_size},
       });
   }

Where the data lands
--------------------

Sinks are configured per metric. The two that ship today:

``GameDBSink``
   Numeric series — counters and gauges — flow into the GameDB
   time-series tables, queryable later via
   :cpp:func:`World::query_time_series` (or its Python binding,
   ``world.query_time_series(name, start, end)``).

``SpdlogSink``
   Structured events flow to a named spdlog logger. The default
   ``"diag_file"`` logger writes JSONL to disk, one line per event,
   for ``jq``-friendly inspection.

The rule of thumb: **numeric → GameDB**, **structured → spdlog**.
Counters that you want to plot stay in GameDB; events you want to
grep stay in JSONL.

Reading the data back
---------------------

Numeric series are queried by name and time range. From Python
(after attaching to a running session):

.. code-block:: python

   # window: last 60 seconds
   import time
   end = int(time.time())
   start = end - 60
   points = world.query_time_series(start, end, series_name="weapons.bullets_fired")
   for ts, value in points:
       print(ts, value)

Structured events are tailed from the JSONL log. From the demo's
working directory:

.. code-block:: bash

   cd aetherion-demo-projects/03_rogue_dungeon/minimal_dungeon
   tail -F logs/diag/diag-*.jsonl \
     | jq -c 'select(.channel == "persistence.save")'

Filter to one channel with ``select(.channel == ...)``; pivot to a
table with ``jq -r '[.timestamp, .slot, .duration_ms] | @tsv'``.

Compile-time disable
--------------------

For paranoid release builds, flip the master switch in
``src/diag/Diag.hpp``:

.. code-block:: cpp

   namespace aetherion::diag {
   inline constexpr bool kEnabled = false;  // every emit body becomes dead code
   } // namespace aetherion::diag

With ``kEnabled = false``, every ``inc()``, ``set()``, and ``log()``
body is short-circuited and the optimiser eliminates it — same
discipline as the existing water-debug compile-time gate. This is
heavier than the runtime
:cpp:func:`aetherion::diag::Registry::disable` knob, which keeps the
machinery in the binary but skips per-flush work.
