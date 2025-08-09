from typing import Any

from pydantic import BaseModel, ConfigDict

from aetherion import BaseEntity


class AccountConnectionMetadata(BaseModel):
    model_config = ConfigDict(arbitrary_types_allowed=True)

    account_id: str
    key: str
    name: str
    description: str
    status: str = "unavailable"  # "available", "unavailable", "deprecated"

    # Connection to the database for this account, if applicable
    account_jwt: Any | None = None  # Placeholder for JWT token object


class BeastConnectionMetadata(BaseModel):
    model_config = ConfigDict(arbitrary_types_allowed=True)

    account_id: str | None = None  # Optional, if this is a beast connection for an account
    key: str
    name: str
    description: str
    world_connection_jwt: Any | None = None  # Placeholder for JWT token object
    entity: BaseEntity | None = None
    status: str = "unavailable"  # "available", "unavailable", "deprecated"
