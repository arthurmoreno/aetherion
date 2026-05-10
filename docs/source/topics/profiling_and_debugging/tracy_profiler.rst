Tracy Profiler
==============

.. note::

   Tracy Profiler integration is **planned, not yet shipped**. This
   page is a placeholder so the navigation tree stays complete; full
   operational documentation will land here once the integration is
   merged.

When live, Tracy will be the right tool for:

- Watching RSS climb in real time during a long session, instead of
  waiting for an at-exit dump from LeakSanitizer.
- Visualising per-frame timing zones as a flame graph, to spot a
  frame-rate decay before it becomes a hitch.
- Attributing allocations to specific tick subsystems on the live
  timeline, paired with the ``aetherion::diag`` counters that hint
  at *where* to zoom in (see :doc:`diagnostic_module`).

Once shipped, the build switch will be ``TRACY=1`` (see
:doc:`profile_builds`). The Tracy viewer connects to the running
engine over UDP — auto-discovery on the local network, no manual
port configuration in the common case.

This page will then cover:

- How to build the engine with ``make build TRACY=1``.
- How to build the Tracy viewer binary from source.
- How to launch and connect (auto-discovery, manual hostname).
- How to find a leak in the Memory tab.
- How to find a slow function in the frame timeline.
- Interpretation tips specific to the Aetherion tick model.
