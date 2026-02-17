from enum import Enum


class PermissionLevel(Enum):
    """Permission levels for different types of actions."""

    GUEST = "guest"
    USER = "user"
    ADMIN = "admin"
