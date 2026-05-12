// Camera/Utils.hpp — dependency-free helpers for the Camera module.
//
// This header intentionally has zero dependencies on the engine's heavy
// types (`EntityInterface`, `WorldView`, `RenderQueue`, FlatBuffers, etc.).
// Helpers that need those types live in `src/CameraUtils.{cpp,hpp}` at the
// top level — that TU is compiled directly into the `_aetherion` module,
// where the rich set of nanobind STL casters from `aetherion.hpp` is in
// scope. Anything in *this* header must remain primitive-only so that the
// Camera static lib's compilation context never pulls in templated
// nanobind machinery that depends on includes outside its own surface.
//
// Plan:
// .claude/docs/epics-plans/2026-05-11-dimetric-tile-walker-cpp-migration.md

#pragma once

// Axis-aligned point-in-rectangle test. Used by the camera-side mouse
// selection helpers in `CameraUtils.cpp` and by the dimetric tile walker
// for hover hit-testing.
bool isMouseWithin(int mx, int my, int x, int y, int width, int height);
