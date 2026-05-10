// Global operator new / operator delete overrides that route every C++
// allocation through Tracy's TracyAllocS / TracyFreeS so call stacks land
// in the profiler's Memory tab and feed the bottom-up + top-down call
// trees.
//
// Why the *S variants*: `TracyAlloc` (no S) only records the allocation
// site; the Memory tab's call-tree views stay empty because Tracy has
// no parent frames to aggregate by. `TracyAllocS(ptr, size, depth)`
// invokes libunwind to capture `depth` frames per allocation, which
// is the precondition for "find the call path that's leaking" to be
// answerable in the UI.
//
// `kCallStackDepth = 15` is mirrored by the project-wide
// `-DTRACY_CALLSTACK=15` define (see CMakeLists.txt's
// AETHERION_TRACY_BUILD block). Aetherion's deepest hot paths bottom
// out below that. Frame walks are not free — measurable but
// acceptable overhead for a leak hunt; the default (non-Tracy) build
// pays nothing because the whole TU is `#ifdef TRACY_ENABLE`.
//
// Plan: .claude/docs/epics-plans/2026-05-09-tracy-profiler-integration.md

#ifdef TRACY_ENABLE

#include <cstdlib>
#include <new>

#include <tracy/Tracy.hpp>

namespace {
// Per-allocation call-stack depth. Keep in sync with
// `-DTRACY_CALLSTACK=15` in the AETHERION_TRACY_BUILD CMake block —
// having one source of truth for the value (the macro) and one for
// the API call (this constant) is intentional: the macro gates Tracy's
// internal capture, this constant feeds the per-call API.
constexpr int kCallStackDepth = 15;
} // namespace

// ─── Sized non-aligned ──────────────────────────────────────────────

void *operator new(std::size_t size) {
  void *ptr = std::malloc(size);
  if (!ptr) {
    throw std::bad_alloc();
  }
  TracyAllocS(ptr, size, kCallStackDepth);
  return ptr;
}

void *operator new[](std::size_t size) {
  void *ptr = std::malloc(size);
  if (!ptr) {
    throw std::bad_alloc();
  }
  TracyAllocS(ptr, size, kCallStackDepth);
  return ptr;
}

void operator delete(void *ptr) noexcept {
  TracyFreeS(ptr, kCallStackDepth);
  std::free(ptr);
}

void operator delete[](void *ptr) noexcept {
  TracyFreeS(ptr, kCallStackDepth);
  std::free(ptr);
}

// Sized delete (C++14). We don't need the size here — Tracy's free API
// only takes the pointer — but we still must define the overload so
// the linker picks it for objects that go through the sized-delete
// path.
void operator delete(void *ptr, std::size_t /*size*/) noexcept {
  TracyFreeS(ptr, kCallStackDepth);
  std::free(ptr);
}

void operator delete[](void *ptr, std::size_t /*size*/) noexcept {
  TracyFreeS(ptr, kCallStackDepth);
  std::free(ptr);
}

// ─── Aligned variants (C++17) ───────────────────────────────────────

void *operator new(std::size_t size, std::align_val_t align) {
  void *ptr = std::aligned_alloc(static_cast<std::size_t>(align), size);
  if (!ptr) {
    throw std::bad_alloc();
  }
  TracyAllocS(ptr, size, kCallStackDepth);
  return ptr;
}

void *operator new[](std::size_t size, std::align_val_t align) {
  void *ptr = std::aligned_alloc(static_cast<std::size_t>(align), size);
  if (!ptr) {
    throw std::bad_alloc();
  }
  TracyAllocS(ptr, size, kCallStackDepth);
  return ptr;
}

void operator delete(void *ptr, std::align_val_t /*align*/) noexcept {
  TracyFreeS(ptr, kCallStackDepth);
  std::free(ptr);
}

void operator delete[](void *ptr, std::align_val_t /*align*/) noexcept {
  TracyFreeS(ptr, kCallStackDepth);
  std::free(ptr);
}

void operator delete(void *ptr, std::size_t /*size*/,
                     std::align_val_t /*align*/) noexcept {
  TracyFreeS(ptr, kCallStackDepth);
  std::free(ptr);
}

void operator delete[](void *ptr, std::size_t /*size*/,
                       std::align_val_t /*align*/) noexcept {
  TracyFreeS(ptr, kCallStackDepth);
  std::free(ptr);
}

#endif // TRACY_ENABLE
