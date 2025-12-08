from functools import partial
from typing import Callable

from aetherion.game_state.state import SharedState


class Scheduler:
    def __init__(self):
        self._functions: set[Callable[[], SharedState]] = set()

    def schedule(self, func: Callable):
        """Add a function to the scheduler."""
        self._functions.add(func)
        func_name = self._get_callable_name(func)
        print(f"Scheduled function: {func_name}")

    def unschedule(self, func: Callable):
        """Remove a function from the scheduler."""
        self._functions.discard(func)
        func_name = self._get_callable_name(func)
        print(f"Unscheduled function: {func_name}")

    def execute_scheduled_funcs(self, shared_state: SharedState) -> SharedState:
        """Execute all scheduled functions."""
        for func in list(self._functions):
            try:
                shared_state = func(shared_state)
            except Exception as e:
                import traceback

                traceback.print_exc()

                breakpoint()
                func_name = self._get_callable_name(func)
                print(f"Error executing {func_name}: {e}")
        return shared_state

    def _get_callable_name(self, func: Callable) -> str:
        """
        Retrieve the name of the callable for logging purposes.
        Supports regular functions, partial functions, and other callables.
        """
        if hasattr(func, "__name__"):
            return func.__name__
        elif isinstance(func, partial):
            # Retrieve the original function's name from the partial object
            original_func_name = getattr(func.func, "__name__", repr(func.func))
            return f"partial({original_func_name})"
        elif hasattr(func, "__class__") and hasattr(func, "__call__"):
            # For callable objects like class instances with __call__
            return func.__class__.__name__
        else:
            # Fallback representation
            return repr(func)
