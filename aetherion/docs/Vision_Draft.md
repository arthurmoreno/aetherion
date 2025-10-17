# Aetherion Engine — Vision, Strategy & Roadmap (Working Doc)

> **Status:** Draft v0.1
> **Author:** <your name>
> **Last updated:** \<YYYY‑MM‑DD>

---

## 1) One‑Page Vision

# Problems Description

Most existing engines that support RPGs in the style of Ultima or Tibia either come locked to those specific universes (through reverse‑engineered private servers) or focus on being general‑purpose and flexible, but this very breadth comes at the cost of providing focused, ready‑made tooling. For developers aiming at Ultima/Tibia‑style RPGs, that translates into a lack of specialized support and several concrete pain points:

Lack of creational freedom: No standalone engines with editors and advanced tooling exist specifically for building Ultima/Tibia‑inspired old‑school RPGs.

Over‑flexibility of general engines: Existing frameworks force procedural generation tools to adapt to countless engine setups, slowing iteration and complicating integration.

Inefficient simulation for niche needs: Simulation‑heavy games often face brittle save data, editors unaware of time‑based AI/routines, and the absence of reusable, easily decoupled discrete physics simulations that could be applied across projects (e.g., world physics and water).

Complex AI development loop: Iterating on advanced AI (evolution, life simulation) in many engines is slow, as they lack native bridges to modern Python AI libraries and scripting flows that feel natural.

# Vision

The goal is to build a purpose‑built engine that faithfully defines and simulates the physics of Tibia‑style RPGs, and extends them into a modern workflow. Instead of generic 3D or sandbox physics, the focus is on tile‑based, discrete mechanics, interactions, and water simulations inspired by Dwarf Fortress and Terraria.

This engine will:

Provide tight integration between procedural generation frameworks and the engine core, so Python algorithms can directly shape worlds without friction.

Offer Python‑based scripting that feels natural and expressive, letting creators describe game systems, worlds, and stories with clarity—like writing the math of a universe.

Enable seamless use of modern AI and evolution libraries for life‑simulation and systemic AI, with fast iteration loops.

Focus on discrete world physics and water simulation, designed for reproducibility and GPU parallelism, capturing the essence of Tibia‑like mechanics.

Deliver an editor and advanced tooling environment tailored for simulation‑heavy games: robust saves, visual tools for time‑centric AI/routines, and optimized sync/server models.

Result: A specialized engine for creators who want to build systemic RPGs and life‑sims grounded in Tibia‑style physics, but powered by modern procedural generation and AI experimentation.

**North Star**
*Ship a small, moddable life‑sim slice created entirely through the editor*, proving fast iteration and durable saves.

**Tenets (non‑negotiables)**

1. **Editor‑first**: engine serves the editor; editor serves iteration speed.
2. **Project isolation**: projects are mountable packs; no cross‑contamination.
3. **Stable saves**: schema‑versioned, auto‑migrated, backwards‑compatible.
4. **Observable sim**: everything inspectable, rewinding is routine.

**Non‑Goals**

* AAA graphics stack; no bespoke GI or cutting‑edge rendering.
* Massive networking/servers at v1.
* General‑purpose engine for all genres.

---

## 2) Product Pillars (what makes it special)

TBD
---

## 3) Target Users & Use Cases

**Primary user**: solo/indie dev prototyping life‑sim, management, or storylet games.
**Secondary**: designers/modders editing schedules, needs, and storylets without touching code.

**Top use cases**

1. Create an NPC routine with needs/moods and preview day/night cycles.
2. Author a storylet graph; inject it into a running save to test outcomes.
3. Switch between Project A and B without rebuilds; content packs stay isolated.
4. Add a mechanic (e.g., weather) and verify old saves still load.

---

## 4) PR/FAQ (Narrative) --- What is this ???

**Press‑style headline**: *"Life‑Sim Engine lets solo devs edit time itself."*
**FAQ** (excerpt)

* *Q: Why make an engine instead of using Unity/UE?*
  A: Time‑centric sim tooling is niche. Owning the stack gives hot‑reload, rewindable sim, and robust saves without middleware friction.
* *Q: How hard is the learning curve?*
  A: Editor first; gameplay composition via assets/graphs; code optional for systems.
* *Q: Can I break my project by updating the engine?*
  A: Engine versions are semver’d; migration scripts run automatically; projects pin versions.

---

## 5) Architecture Overview (high level)

```
/engine
  core/        (math, ecs/jobs, serialization)
  render/      (backend‑agnostic; GL/Vulkan adapters later)
  assets/      (importers, cache, incremental pipeline)
  scripting/   (Lua or Python bridge; hot‑reload)
  sim/         (time, needs/moods, schedules, state machines)
  story/       (storylet/quest graph runtime)
  editor/      (panels, inspectors, timeline, graph views)
  plugins/     (physics, audio, navmesh)
/games
  life‑sim/    (content packs, systems; no editor code)
/samples      (golden toy projects)
```

**Key interfaces**

* **IProjectPack** (mount/unmount, deps, version).
* **ISaveSchema** (versioned, migrators, diff).
* **ISimTick** (advance, snapshot/restore).
* **IHotReload** (watchers, apply, rollback on failure).

---

## 6) Editor UX (MVP panels)

* **Project Switcher** (mount packs, pin engine version, dependency graph).
* **Scene/Entity Inspector** (ECS view; live variables watch).
* **Simulation Timeline** (time controls, record/scrub, snapshot list).
* **Schedule Editor** (NPC routines; calendar + state machine).
* **Storylet Graph** (conditions, rewards, test‑injection).
* **Save Manager** (schema versions, diff, migrate, rollback).
* **Console & Telemetry** (performance, hot‑reload logs).

---

## 7) Roadmap

### 0–6 Weeks (MVP Slice)

* [ ] Engine skeleton with module boundaries (core/assets/sim/editor).
* [ ] ECS + basic serialization; save/load v0.1.
* [ ] Editor shell: project loader, Scene view, Inspector.
* [ ] Simulation time service + pause/play/step.
* [ ] Hot‑reload data for NPC schedules.
* [ ] Golden Projects: **Toy2D**, **LifeSim‑Slice** (NPC + routine).
* [ ] CI: headless tests + 5‑min editor smoke test.

### 6–12 Weeks (Usable Editor)

* [ ] Schedule Editor panel; NPC day/night loop.
* [ ] Snapshot/rewind MVP (per‑N ticks).
* [ ] Save schema v0.2 + migrator pipeline.
* [ ] Storylet Graph editor (read‑only runtime).
* [ ] Project Switcher v0.1 (isolation, per‑project settings).
* [ ] Metrics overlay (frame time, rebuild times).

### 3–6 Months (Creator Beta)

* [ ] Storylet authoring + live injection.
* [ ] Robust save diffs; conflict resolver.
* [ ] Content importers (tiles, sprites, audio); incremental builds.
* [ ] Plugin system (physics/audio).
* [ ] Packaging: share a project pack; pin engine version.
* [ ] Documentation site + examples.

---

## 8) Success Metrics (lead/lag)

**Lead indicators** (weekly):

* Median editor cold start time ≤ X sec.
* Hot‑reload cycle time ≤ Y sec (data → live).
* % crash‑free sessions ≥ 99%.
* Golden Projects load & play: 100%.

**Lag indicators** (monthly):

* # of features prototyped end‑to‑end.
* # external testers shipping small scenes/storylets.
* Save migration failures: 0.

---

## 9) Decision Framework (v1.0)

For any task (feature/refactor), check:

* User value this week?
* Maintenance reduction?
* Risk burn‑down?
* Locality of change?
* Testability?
  **Rule**: ≥3 “Yes” → Do now. ≤2 → Backlog with note.

**Additional guardrails**

* Max refactor timebox: 2 days before checkpoint.
* At least one gameplay‑facing win per week.
* No changes that touch >5 modules without a feature flag.

---

## 10) Risks & Mitigations

* **Scope creep** → Strict MVP panels; storylet authoring read‑only first.
* **Save corruption** → Versioned schemas, migrators, snapshot autos.
* **Tooling fatigue** → Monthly demo day; record GIFs; share devlog.
* **Performance traps** → Telemetry overlay + budgets for rebuild times.

---

## 11) Open Questions

* Scripting language choice (Lua vs. Python/C#)?
* Rendering backend path (GL now, Vulkan later?): impact on editor.
* Storylet format standardization?
* Rewind storage cost vs. fidelity.

---

## 12) Glossary

* **Storylet**: small conditional narrative chunk with triggers/outcomes.
* **Pack**: mountable project containing assets, data, and config.
* **Snapshot**: captured sim state used for rewind/compare.

---

## 13) Appendix: Templates

### A) Feature Brief (≤1 page)

* **Goal**: \<what changes for the user?>
* **Non‑Goals**: <out of scope>
* **Design Sketch**: \<UI/state outline or diagram>
* **Interfaces**: <public API signatures>
* **Risks**: \<top 3>
* **Tests**: \<unit, integration, editor smoke>
* **Cutline**: <what ships if time runs out>

### B) Migration Note

* **From/To**: schema vX → vY
* **Script**: <migration steps>
* **Backout**: <rollback path>

### C) Devlog Changelog

* **Version**: 0.12.0
* **Highlights**: \<GIFs/notes>
* **Breaking Changes**: <list>
* **Migration**: <link>

---

## 14) Example Filled‑In (brief)

**Vision**: "Edit time itself."
**Pillars**: Timeline • Rewind • Hot‑Reload • Stable Saves • Project Switcher
**MVP**: NPC with 4‑state routine; editable in Schedule Editor; hot‑reload into a running day/night loop; snapshot every 5s; saves survive schema v0.1→v0.2.
