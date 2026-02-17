"""
ResourceManager: a simple registry for Python resources (e.g., Sprite objects, images, fonts)
"""


class ResourceManager:
    """
    A registry to hold and retrieve shared resource instances by key.
    """

    def __init__(self):
        self._resources: dict[str, object] = {}

    def register(self, key: str, resource: object) -> None:
        """
        Register a resource under a unique key. Overwrites if key already exists.

        Args:
            key: Identifier for the resource.
            resource: The resource object to store.
        """
        self._resources[key] = resource

    def get(self, key: str) -> object | None:
        """
        Retrieve a registered resource by key.

        Args:
            key: Identifier of the resource.

        Returns:
            The registered resource, or None if not found.
        """
        return self._resources.get(key)

    def has(self, key: str) -> bool:
        """
        Check if a resource is registered under the given key.
        """
        return key in self._resources

    def unregister(self, key: str) -> None:
        """
        Remove a resource from the registry.
        """
        self._resources.pop(key, None)

    def clear(self) -> None:
        """
        Remove all registered resources.
        """
        self._resources.clear()


# Provide a module-level singleton for convenience
manager = ResourceManager()
