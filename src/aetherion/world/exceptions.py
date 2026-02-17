class WorldException(Exception):
    """Base class for all exceptions related to the world."""

    pass


class CreatePerceptionResponseException(WorldException):
    """Exception raised when creating a perception response fails."""

    def __init__(self, message: str = "Failed to create perception response.") -> None:
        super().__init__(message)
