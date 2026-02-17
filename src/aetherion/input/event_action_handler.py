from typing import Any, Callable

from pydantic import BaseModel


class InputEventActionHandler(BaseModel):
    """
    A class to handle input events.
    This class is used to map input events to specific actions.
    """

    handler: Callable[[], None]
    kwargs: dict[str, Any] = {}
    processor_kwargs: set[str] = set()  # Used to pass shared state or other context if needed
