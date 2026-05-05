#ifndef EVENT_SINK_HPP
#define EVENT_SINK_HPP

#include <entt/entt.hpp>

#include <cassert>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

// Cross-thread event submission for EnTT's non-thread-safe dispatcher.
// Engines depend on `EventSink &`; the sink routes by calling thread:
//   * main thread  → `entt::dispatcher::enqueue<T>` directly
//   * any other    → `WorkerEventSink` (mutex-protected staging buffer)
// `World` drains the staging buffer at the top of each tick, before
// `dispatcher.update()`. See plan
// `.claude/docs/epics-plans/2026-05-04-event-sink-architecture.md`.

class WorkerEventSink {
public:
  WorkerEventSink() = default;
  WorkerEventSink(const WorkerEventSink &) = delete;
  WorkerEventSink &operator=(const WorkerEventSink &) = delete;

  // Variadic to mirror `entt::dispatcher::enqueue<T>(args...)` — constructs
  // `T` from forwarded args at call time and stages the constructed value.
  // Paren-init (not brace-init) matches EnTT's dispatcher contract and allows
  // the int → float narrowing that real call sites rely on (e.g. passing the
  // integer literal `0` for a `float` velocity component).
  template <typename T, typename... Args> void enqueue(Args &&...args) {
    T event(std::forward<Args>(args)...);
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.emplace_back(
        [captured = std::move(event)](entt::dispatcher &dispatcher) mutable {
          dispatcher.enqueue<T>(std::move(captured));
        });
  }

  // Move pending events into the dispatcher. Replays without holding the
  // mutex so worker threads can keep enqueueing while the replay runs.
  void drain(entt::dispatcher &dispatcher) {
    std::vector<std::function<void(entt::dispatcher &)>> local;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      std::swap(local, pending_);
    }
    for (auto &fn : local) {
      fn(dispatcher);
    }
  }

private:
  std::mutex mutex_;
  std::vector<std::function<void(entt::dispatcher &)>> pending_;
};

class EventSink {
public:
  EventSink(entt::dispatcher &dispatcher, WorkerEventSink &staging,
            std::thread::id main_thread_id)
      : dispatcher_(&dispatcher), staging_(&staging),
        main_thread_id_(main_thread_id) {}

  template <typename T, typename... Args> void enqueue(Args &&...args) {
    if (std::this_thread::get_id() == main_thread_id_) {
      dispatcher_->enqueue<T>(std::forward<Args>(args)...);
    } else {
      staging_->enqueue<T>(std::forward<Args>(args)...);
    }
  }

  // Handler registration paths still need the raw dispatcher. MUST be called
  // only from the main thread (enforced by contract — checked in debug
  // builds via `assert`).
  entt::dispatcher &raw_dispatcher_main_only() {
    assert(std::this_thread::get_id() == main_thread_id_ &&
           "EventSink::raw_dispatcher_main_only called from non-main thread");
    return *dispatcher_;
  }

private:
  entt::dispatcher *dispatcher_;
  WorkerEventSink *staging_;
  std::thread::id main_thread_id_;
};

#endif // EVENT_SINK_HPP
