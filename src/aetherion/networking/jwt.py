from __future__ import annotations

from datetime import datetime, timedelta, timezone

import jwt
from aetherion.logger import logger

from aetherion.networking.constants import PermissionLevel
from aetherion.networking.exceptions import AuthenticationError


class JWTAuthenticator:
    """Handles JWT token validation and user authentication."""

    def __init__(self, secret_key: str = "dev-secret-key", algorithm: str = "HS256") -> None:
        self.secret_key: str = secret_key
        self.algorithm: str = algorithm

    def validate_token(self, token: str) -> dict[str, str | int | float]:
        """Validate JWT token and return user claims."""
        try:
            payload = jwt.decode(token, self.secret_key, algorithms=[self.algorithm])

            # Check token expiration
            if "exp" in payload:
                exp_timestamp = payload["exp"]
                if datetime.now(timezone.utc).timestamp() > exp_timestamp:
                    raise AuthenticationError("Token has expired")

            return payload

        except jwt.InvalidTokenError as e:
            raise AuthenticationError(f"Invalid token: {str(e)}")

    def get_user_permission_level(self, user_claims: dict[str, str | int | float]) -> PermissionLevel:
        """Extract user permission level from claims."""
        role_value = user_claims.get("role", "guest")
        if isinstance(role_value, str):
            role = role_value.lower()
        else:
            role = "guest"

        try:
            return PermissionLevel(role)
        except ValueError:
            logger.warning(f"Unknown role '{role}', defaulting to GUEST")
            return PermissionLevel.GUEST

    def check_action_permission(self, action_name: str, permission_level: PermissionLevel) -> bool:
        """Check if the user has permission to perform the given action."""
        # Define action permission requirements
        action_permissions = {
            # Guest level actions
            "ping": PermissionLevel.GUEST,
            # User level actions
            "subscribe_entities": PermissionLevel.USER,
            "subscribe_state": PermissionLevel.USER,
            "unsubscribe_entities": PermissionLevel.USER,
            "walk_left_entity": PermissionLevel.USER,
            "walk_up_entity": PermissionLevel.USER,
            "walk_right_entity": PermissionLevel.USER,
            "walk_down_entity": PermissionLevel.USER,
            "jump_entity": PermissionLevel.USER,
            "make_entity_eat": PermissionLevel.USER,
            "create_player": PermissionLevel.USER,
            "action_and_state_request": PermissionLevel.USER,
            "ai_metadata": PermissionLevel.USER,
            # Admin level actions
            "ai_decisions": PermissionLevel.ADMIN,
        }

        required_level = action_permissions.get(action_name, PermissionLevel.ADMIN)

        # Check if user's permission level is sufficient
        level_hierarchy = {
            PermissionLevel.GUEST: 0,
            PermissionLevel.USER: 1,
            PermissionLevel.ADMIN: 2,
        }

        return level_hierarchy.get(permission_level, 0) >= level_hierarchy.get(required_level, 2)


class MockJWTProvider:
    """Provides mock JWT tokens for local development."""

    def __init__(self, secret_key: str = "dev-secret-key", algorithm: str = "HS256") -> None:
        self.secret_key: str = secret_key
        self.algorithm: str = algorithm

    def create_mock_token(self, user_id: str, role: str = "user", expires_in_hours: int = 24) -> str:
        """Create a mock JWT token for testing."""
        payload = {
            "user_id": user_id,
            "role": role,
            "iat": datetime.now(timezone.utc),
            "exp": datetime.now(timezone.utc) + timedelta(hours=expires_in_hours),
        }

        return jwt.encode(payload, self.secret_key, algorithm=self.algorithm)

    def get_guest_token(self) -> str:
        """Get a guest token for minimal access."""
        return self.create_mock_token("guest_user", "guest", 1)

    def get_user_token(self) -> str:
        """Get a standard user token."""
        return self.create_mock_token("test_user", "user", 24)

    def get_admin_token(self) -> str:
        """Get an admin token for full access."""
        return self.create_mock_token("admin_user", "admin", 24)
