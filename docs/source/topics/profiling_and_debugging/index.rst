Profiling & Debugging
=====================

Long-running simulations leak memory and decay framerate. This
section is the playbook: which tool to reach for first, in what
order, and what to look for.

The engine ships durable in-process instrumentation
(:doc:`diagnostic_module`), one-shot build switches that prepare the
binary for external tools (:doc:`profile_builds`), and a curated set
of profiler recipes (:doc:`external_profilers`,
:doc:`tracy_profiler`). Two end-to-end :doc:`case_studies` thread
those pieces together against real investigations.

Decision tree — which tool first?
---------------------------------

Triage by symptom:

.. admonition:: "RSS is climbing during normal play, no crash."
   :class: tip

   Start with ``aetherion::diag`` gauges (:doc:`diagnostic_module`):
   sample ``rss_mb`` plus a few subsystem-level byte counters at
   1 Hz, plot the series, see which subsystem is growing. If
   nothing in the per-subsystem series correlates with the climb,
   move to ``valgrind --tool=massif --pages-as-heap=yes`` (see
   :doc:`external_profilers`). Once the
   :doc:`profile_builds` ``SANITIZE=address`` switch ships,
   prefer it as the first heavyweight step.

.. admonition:: "Need to attribute a leak to a specific call stack."
   :class: tip

   AddressSanitizer first (:doc:`profile_builds`), then Tracy if
   you want a live timeline (:doc:`tracy_profiler`), then valgrind
   massif as the heavy fallback (:doc:`external_profilers`). Each
   step is slower and noisier than the previous, but each catches
   things the previous missed.

.. admonition:: "Frame rate drops over a long session."
   :class: tip

   Tracy's frame profiler when shipped (:doc:`tracy_profiler`).
   Until then: hand-instrument the suspect tick subsystems with
   ``aetherion::diag`` gauges (:doc:`diagnostic_module`) holding
   per-frame elapsed time, and watch the series for the regression
   in shape. ``perf record`` (:doc:`external_profilers`) is the
   right next step once you've narrowed by subsystem.

.. admonition:: "Use-after-free or crash on shutdown."
   :class: tip

   AddressSanitizer (:doc:`profile_builds`) once shipped, or
   valgrind memcheck in the meantime
   (:doc:`external_profilers`). Either tool will identify the
   freed-but-still-referenced allocation by site.

Throughout: the durable instrumentation in
:doc:`diagnostic_module` is the substrate. Every external tool
listed here is a way to *attribute* what the diag counters and
gauges have already *detected*.

.. toctree::
   :maxdepth: 2
   :caption: Contents

   diagnostic_module
   profile_builds
   external_profilers
   tracy_profiler
   case_studies
