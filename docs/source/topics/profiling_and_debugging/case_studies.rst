Case Studies
============

Two end-to-end walks of the methodology — symptom, diagnosis, fix,
verification — drawn from real investigations against long-running
Aetherion-based simulations. Each case shows which tool answered
which question, and in what order.

Case study 1 — Per-tick unbounded cache leak
--------------------------------------------

Symptom
~~~~~~~

In a long-running session of an Aetherion-based simulation, RSS
climbed by ~10 MB per 10-second sampler interval. Growth was
**continuous** (not staircased to player actions) and **plateaued
when the simulation idled** — no entities moving, no events
firing — which ruled out any per-event allocator path.

Diagnosis
~~~~~~~~~

The plateau was the load-bearing observation: it pointed at
*tick-driven* allocation, not event-driven. Order of investigation:

1. **Confirm with diag gauges.** A ``Gauge`` was already wired for
   ``rss_mb`` and ``terrain_grid_bytes`` at one-Hz cadence (see
   :doc:`diagnostic_module`). The plotted series showed ``rss_mb``
   climbing linearly while ``terrain_grid_bytes`` stayed flat —
   leak was *not* in the terrain grid.
2. **No-entities experiment.** Launched the demo with the world
   pre-populated but every entity removed; the ``rss_mb`` series
   kept climbing at the same rate. Per-entity allocation paths
   were ruled out.
3. **Heap attribution with massif (pages-as-heap mode).** Default
   mode showed a flat profile — the allocator was reusing existing
   arena pages, so ``malloc`` calls weren't growing. Switched to
   ``--pages-as-heap=yes`` (see :doc:`external_profilers`); the
   tree pointed at one C++ translation unit's per-tick code path.
4. **Code reading.** The translation unit owned a singleton
   ``std::map`` cache fed once per tick, with no eviction. A
   one-line debug leftover — ``needsSync = false`` slipped in
   during an unrelated investigation — had silently disabled the
   downstream sync path that would have bounded the cache. The map
   grew unbounded; the allocator faithfully gave it more arena
   pages; pages-as-heap mode was the only tool that could see them.

Fix
~~~

Three changes, smallest first:

- Restored the sync invariant by removing the debug-leftover line.
- Bounded the in-memory cache: ``std::map::erase(begin())`` when the
  size exceeded a configurable cap, so even a future regression in
  the sync path can't blow the budget.
- Added a SQLite ``AFTER INSERT`` trigger to bound the on-disk side
  in lock-step with the in-memory cap, so retention is enforced at
  both tiers.

Verification
~~~~~~~~~~~~

Re-ran the ``rss_mb`` sampler. RSS plateaued within one or two
warmup cycles instead of climbing linearly. The diag series
(:doc:`diagnostic_module`) was the same instrument used to confirm
the symptom and the fix — closing the loop with one tool.

What made the diagnosis fast
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The durable infrastructure: ``aetherion::diag`` gauges were already
sampling ``rss_mb`` at one-Hz cadence before the leak hunt began, so
the climb rate was a known-good baseline rather than a number
gathered ad hoc once the report came in. Without that sampler, step
1 alone would have taken an afternoon.

Case study 2 — Per-frame ImGui scene-graph duplication
-------------------------------------------------------

Symptom
~~~~~~~

A profile-build run crashed on the first frame with the ImGui
assertion::

   Assertion (g.FrameCount == 0 || g.FrameCountEnded == g.FrameCount) "Forgot to call Render() or EndFrame() at the end of the previous frame?"

The release build did **not** crash — the same code had been running
in production for weeks.

Diagnosis
~~~~~~~~~

The release build did not crash because ``-DNDEBUG`` compiled out
``IM_ASSERT``. The bug — calling ImGui's per-scene draw setup twice
per frame, once eagerly in a scene's ``__init__`` and again from the
SceneManager-driven path — was silently leaking ImGui's per-frame
draw lists in production. No assert meant no signal; the leak only
showed in a long session as growing RSS, which had been written off
as expected texture caching.

The profile build (see :doc:`profile_builds`) re-enables
``IM_ASSERT`` because it runs in ``RelWithDebInfo`` mode without
``-DNDEBUG``. The crash on first frame was the first time the
duplication had ever surfaced as a test signal.

Fix
~~~

Removed the eager ``__init__``-time scene-graph load. Only the
SceneManager-driven path survives, restoring the documented
"scenes load on ``change_scene``" contract.

Verification
~~~~~~~~~~~~

Profile build ran past first frame without asserting. A diag
``Gauge`` measuring per-tick allocation rate (before and after the
fix) showed a measurable drop, confirming the duplicate ImGui setup
was indeed allocating per frame in the release build too.

What made the diagnosis fast
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The same theme: the durable infrastructure. The
``PROFILE=1`` build flag (see :doc:`profile_builds`) was the *only*
configuration that could surface this bug — the release build
silently leaked, and a vanilla debug build would have collapsed
under the demo's frame budget. A maintained profile-build path
turned a six-week-old silent leak into a first-frame assert.
