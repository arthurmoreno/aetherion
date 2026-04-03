from unittest.mock import AsyncMock, MagicMock, patch

import msgpack
import pytest

from aetherion.networking.constants import PermissionLevel
from aetherion.networking.jwt_auth import AuthenticationError
from aetherion.networking.websocket_server import AuthenticatedWebSocketServer, WebSocketGameServer
from aetherion.world.protocols import WorldInterfaceProtocol


@pytest.fixture
def mock_world_interface():
    # dir(Protocol) omits annotation-only members like `world`; __protocol_attrs__ is complete.
    mock_world = MagicMock(spec=list(WorldInterfaceProtocol.__protocol_attrs__))
    mock_world.world.get_entity_by_id.return_value = MagicMock()
    mock_world.last_state_response = None
    mock_world.get_ai_metadata.return_value = {}
    mock_world.register_connected_entity = AsyncMock()
    mock_world.register_list_of_connected_entities = AsyncMock()
    mock_world.unregister_connected_entity = MagicMock()
    mock_world.get_perception_responses_async = AsyncMock(return_value=b"compressed_state_data")
    return mock_world


@pytest.fixture
def mock_websocket():
    ws = AsyncMock()
    ws.remote_address = ("127.0.0.1", 12345)
    ws.state = "OPEN"  # Mock the state attribute
    return ws


@pytest.fixture
def mock_jwt_authenticator():
    # Patch where AuthenticatedWebSocketServer resolves the name (already imported).
    with patch("aetherion.networking.websocket_server.JWTAuthenticator", autospec=True) as MockAuthenticator:
        authenticator = MockAuthenticator.return_value
        authenticator.validate_token.return_value = {
            "user_id": "test_user",
            "role": "player",
            "permission_level": PermissionLevel.USER.value,
        }
        authenticator.get_user_permission_level.return_value = PermissionLevel.USER
        authenticator.check_action_permission.return_value = True
        yield authenticator


@pytest.fixture
def authenticated_server(mock_world_interface, mock_jwt_authenticator):
    return AuthenticatedWebSocketServer(world_interface=mock_world_interface, jwt_secret="test_secret")


@pytest.mark.asyncio
async def test_authenticated_server_init(mock_world_interface, mock_jwt_authenticator):
    server = AuthenticatedWebSocketServer(world_interface=mock_world_interface, jwt_secret="test_secret")
    assert isinstance(server, AuthenticatedWebSocketServer)
    assert server.world_interface == mock_world_interface
    assert server.authenticator == mock_jwt_authenticator
    assert server.authenticated_sessions == {}


@pytest.mark.asyncio
async def test_send_error_response(authenticated_server, mock_websocket):
    await authenticated_server.send_error_response(mock_websocket, "Test Error", "test_error_type")
    mock_websocket.send.assert_called_once()
    sent_data = mock_websocket.send.call_args[0][0]
    unpacked_data = msgpack.unpackb(sent_data, raw=False, strict_map_key=False)
    assert unpacked_data == {
        "type": "error",
        "error_type": "test_error_type",
        "message": "Test Error",
        "success": False,
    }


@pytest.mark.asyncio
async def test_authenticate_request_no_token(authenticated_server, mock_websocket):
    request_message = {"type": "action", "action_name": "walk_left_entity"}
    user_claims = await authenticated_server.authenticate_request(request_message, mock_websocket)
    assert user_claims is None
    mock_websocket.send.assert_called_once()
    sent_data = mock_websocket.send.call_args[0][0]
    unpacked_data = msgpack.unpackb(sent_data, raw=False, strict_map_key=False)
    assert unpacked_data["error_type"] == "authentication_error"
    assert unpacked_data["message"] == "No authentication token provided"


@pytest.mark.asyncio
async def test_authenticate_request_invalid_token(authenticated_server, mock_websocket, mock_jwt_authenticator):
    mock_jwt_authenticator.validate_token.side_effect = AuthenticationError("Invalid Token")
    request_message = {"type": "action", "action_name": "walk_left_entity", "token": "invalid_jwt"}
    user_claims = await authenticated_server.authenticate_request(request_message, mock_websocket)
    assert user_claims is None
    mock_jwt_authenticator.validate_token.assert_called_once_with("invalid_jwt")
    mock_websocket.send.assert_called_once()
    sent_data = mock_websocket.send.call_args[0][0]
    unpacked_data = msgpack.unpackb(sent_data, raw=False, strict_map_key=False)
    assert unpacked_data["error_type"] == "authentication_error"
    assert unpacked_data["message"] == "Invalid Token"


@pytest.mark.asyncio
async def test_authenticate_request_valid_token(authenticated_server, mock_websocket, mock_jwt_authenticator):
    expected_claims = {"user_id": "test_user", "role": "player"}
    mock_jwt_authenticator.validate_token.return_value = expected_claims
    request_message = {"type": "action", "action_name": "walk_left_entity", "token": "valid_jwt"}
    user_claims = await authenticated_server.authenticate_request(request_message, mock_websocket)
    assert user_claims == expected_claims
    mock_jwt_authenticator.validate_token.assert_called_once_with("valid_jwt")
    assert mock_websocket not in authenticated_server.authenticated_sessions
    mock_websocket.send.assert_not_called()


@pytest.mark.asyncio
async def test_check_action_authorization_unauthorized(authenticated_server, mock_websocket, mock_jwt_authenticator):
    mock_jwt_authenticator.check_action_permission.return_value = False
    user_claims = {"user_id": "test_user", "role": "player"}
    is_authorized = await authenticated_server.check_action_authorization(
        "restricted_action", user_claims, mock_websocket
    )
    assert not is_authorized
    mock_jwt_authenticator.get_user_permission_level.assert_called_once_with(user_claims)
    mock_jwt_authenticator.check_action_permission.assert_called_once_with("restricted_action", PermissionLevel.USER)
    mock_websocket.send.assert_called_once()
    sent_data = mock_websocket.send.call_args[0][0]
    unpacked_data = msgpack.unpackb(sent_data, raw=False, strict_map_key=False)
    assert unpacked_data["error_type"] == "authorization_error"
    assert "Insufficient permissions" in unpacked_data["message"]


@pytest.mark.asyncio
async def test_check_action_authorization_authorized(authenticated_server, mock_websocket, mock_jwt_authenticator):
    mock_jwt_authenticator.check_action_permission.return_value = True
    user_claims = {"user_id": "test_user", "role": "player"}
    is_authorized = await authenticated_server.check_action_authorization("allowed_action", user_claims, mock_websocket)
    assert is_authorized
    mock_jwt_authenticator.get_user_permission_level.assert_called_once_with(user_claims)
    mock_jwt_authenticator.check_action_permission.assert_called_once_with("allowed_action", PermissionLevel.USER)
    mock_websocket.send.assert_not_called()


@pytest.mark.asyncio
async def test_process_authenticated_message_unauthenticated(authenticated_server, mock_websocket):
    # No token in request, authenticate_request should handle it and return None
    request_message = {"type": "action", "action_name": "walk_left_entity"}
    await authenticated_server.process_authenticated_message(request_message, mock_websocket)
    # send_error_response will be called by authenticate_request
    mock_websocket.send.assert_called_once()
    assert mock_websocket not in authenticated_server.authenticated_sessions


@pytest.mark.asyncio
async def test_process_authenticated_message_unauthorized(authenticated_server, mock_websocket, mock_jwt_authenticator):
    mock_jwt_authenticator.check_action_permission.return_value = False
    mock_jwt_authenticator.validate_token.return_value = {
        "user_id": "test_user",
        "role": "player",
        "permission_level": PermissionLevel.USER.value,
    }
    request_message = {"type": "action", "action_name": "restricted_action", "token": "valid_jwt"}

    # Mock super().process_message to ensure it's not called
    with patch.object(WebSocketGameServer, "process_message", new_callable=AsyncMock) as mock_super_process_message:
        await authenticated_server.process_authenticated_message(request_message, mock_websocket)
        mock_websocket.send.assert_called_once()  # For authorization error
        mock_super_process_message.assert_not_called()
        assert authenticated_server.authenticated_sessions[mock_websocket]["user_id"] == "test_user"


@pytest.mark.asyncio
async def test_process_authenticated_message_authorized(authenticated_server, mock_websocket, mock_jwt_authenticator):
    mock_jwt_authenticator.check_action_permission.return_value = True
    mock_jwt_authenticator.validate_token.return_value = {
        "user_id": "test_user",
        "role": "player",
        "permission_level": PermissionLevel.USER.value,
    }
    request_message = {"type": "action", "action_name": "allowed_action", "token": "valid_jwt"}

    with patch.object(WebSocketGameServer, "process_message", new_callable=AsyncMock) as mock_super_process_message:
        await authenticated_server.process_authenticated_message(request_message, mock_websocket)
        mock_super_process_message.assert_called_once_with(request_message, mock_websocket)
        mock_websocket.send.assert_not_called()
        assert authenticated_server.authenticated_sessions[mock_websocket]["user_id"] == "test_user"


@pytest.mark.asyncio
async def test_process_message_calls_process_authenticated_message(authenticated_server, mock_websocket):
    request_message = {"type": "ping", "token": "dummy_token"}
    with patch.object(
        authenticated_server, "process_authenticated_message", new_callable=AsyncMock
    ) as mock_auth_process:
        await authenticated_server.process_message(request_message, mock_websocket)
        mock_auth_process.assert_called_once_with(request_message, mock_websocket)


@pytest.mark.asyncio
async def test_remove_client(authenticated_server, mock_websocket):
    authenticated_server.add_client(mock_websocket)  # Add client to base class clients set
    authenticated_server.authenticated_sessions[mock_websocket] = {"user_id": "test_user"}
    assert mock_websocket in authenticated_server.clients  # Check if in base class clients
    assert mock_websocket in authenticated_server.authenticated_sessions

    authenticated_server.remove_client(mock_websocket)

    assert mock_websocket not in authenticated_server.clients
    assert mock_websocket not in authenticated_server.authenticated_sessions


@pytest.mark.asyncio
async def test_get_authenticated_user(authenticated_server, mock_websocket):
    user_claims = {"user_id": "test_user", "role": "player"}
    authenticated_server.authenticated_sessions[mock_websocket] = user_claims
    retrieved_claims = authenticated_server.get_authenticated_user(mock_websocket)
    assert retrieved_claims == user_claims

    # Test for non-existent client
    mock_other_websocket = AsyncMock()
    retrieved_claims = authenticated_server.get_authenticated_user(mock_other_websocket)
    assert retrieved_claims is None


class _AsyncIterWebSocket:
    """Minimal connection stub: ``handle_client`` iterates with ``async for data in websocket``."""

    def __init__(self, payloads: list[bytes]) -> None:
        self._payloads = payloads
        self._i = 0
        self.remote_address = ("127.0.0.1", 12345)
        self.state = "OPEN"
        self.send = AsyncMock()

    def __aiter__(self) -> "_AsyncIterWebSocket":
        self._i = 0
        return self

    async def __anext__(self) -> bytes:
        if self._i >= len(self._payloads):
            raise StopAsyncIteration
        p = self._payloads[self._i]
        self._i += 1
        return p


@pytest.mark.asyncio
async def test_handle_client_authentication_flow_success(authenticated_server, mock_jwt_authenticator):
    valid_token = "valid_jwt"
    authenticated_claims = {"user_id": "test_user", "role": "player", "permission_level": PermissionLevel.USER.value}
    mock_jwt_authenticator.validate_token.return_value = authenticated_claims
    mock_jwt_authenticator.check_action_permission.return_value = True

    auth_message = msgpack.packb(
        {
            "type": "action",
            "action_name": "create_player",
            "token": valid_token,
            "player_start_x": 10,
            "player_start_y": 20,
        }
    )
    ping_message = msgpack.packb({"type": "ping", "token": valid_token})

    ws = _AsyncIterWebSocket([auth_message, ping_message])

    with patch.object(WebSocketGameServer, "process_message", new_callable=AsyncMock) as mock_super_process_message:
        await authenticated_server.handle_client(ws)

        mock_jwt_authenticator.validate_token.assert_any_call(valid_token)
        mock_jwt_authenticator.check_action_permission.assert_any_call("create_player", PermissionLevel.USER)
        mock_jwt_authenticator.check_action_permission.assert_any_call("ping", PermissionLevel.USER)
        assert mock_super_process_message.call_count == 2  # Once for create_player, once for ping
        # Disconnect runs _unsubscribe_state → one unsubscribe_state_ack (not an error)
        assert len(ws.send.call_args_list) == 1
        ack = msgpack.unpackb(ws.send.call_args_list[0].args[0], raw=False, strict_map_key=False)
        assert ack == {"type": "unsubscribe_state_ack", "success": True}
        # handle_client's finally calls remove_client, which drops the session
        assert ws not in authenticated_server.authenticated_sessions
