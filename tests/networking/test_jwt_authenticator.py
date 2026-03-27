from __future__ import annotations

import pytest

from aetherion.networking import jwt as jwt_module
from aetherion.networking.constants import PermissionLevel
from aetherion.networking.exceptions import AuthenticationError
from aetherion.networking.jwt import JWTAuthenticator, MockJWTProvider


def test_validate_token_returns_payload_for_valid_token(monkeypatch):
    auth = JWTAuthenticator(secret_key="k")
    monkeypatch.setattr(
        jwt_module.jwt,
        "decode",
        lambda *_args, **_kwargs: {"user_id": "u1", "role": "user"},
        raising=False,
    )

    payload = auth.validate_token("fake-token")
    assert payload["user_id"] == "u1"
    assert payload["role"] == "user"


def test_validate_token_raises_authentication_error_for_invalid_token(monkeypatch):
    class _InvalidTokenError(Exception):
        pass

    auth = JWTAuthenticator(secret_key="k")
    monkeypatch.setattr(jwt_module.jwt, "InvalidTokenError", _InvalidTokenError, raising=False)

    def _decode(*_args, **_kwargs):
        raise _InvalidTokenError("invalid")

    monkeypatch.setattr(jwt_module.jwt, "decode", _decode, raising=False)
    with pytest.raises(AuthenticationError):
        auth.validate_token("not-a-token")


def test_get_user_permission_level_defaults_unknown_role_to_guest():
    auth = JWTAuthenticator()
    permission = auth.get_user_permission_level({"role": "superadmin"})
    assert permission == PermissionLevel.GUEST


def test_check_action_permission_hierarchy_and_default_admin_requirement():
    auth = JWTAuthenticator()

    assert auth.check_action_permission("ping", PermissionLevel.GUEST) is True
    assert auth.check_action_permission("walk_left_entity", PermissionLevel.GUEST) is False
    assert auth.check_action_permission("walk_left_entity", PermissionLevel.USER) is True
    assert auth.check_action_permission("ai_decisions", PermissionLevel.USER) is False
    assert auth.check_action_permission("unknown_action", PermissionLevel.ADMIN) is True


def test_mock_jwt_provider_creates_decodable_role_claim(monkeypatch):
    class _InvalidTokenError(Exception):
        pass

    provider = MockJWTProvider(secret_key="k")
    auth = JWTAuthenticator(secret_key="k")
    monkeypatch.setattr(jwt_module.jwt, "encode", lambda payload, *_args, **_kwargs: payload, raising=False)
    monkeypatch.setattr(
        jwt_module.jwt,
        "decode",
        lambda token, *_args, **_kwargs: {"user_id": token["user_id"], "role": token["role"], "exp": 9999999999},
        raising=False,
    )
    monkeypatch.setattr(jwt_module.jwt, "InvalidTokenError", _InvalidTokenError, raising=False)

    token = provider.create_mock_token("u42", role="admin", expires_in_hours=1)
    payload = auth.validate_token(token)

    assert payload["user_id"] == "u42"
    assert payload["role"] == "admin"
