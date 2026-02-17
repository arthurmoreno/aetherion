from pydantic import BaseModel

from aetherion.constants import WorldInstanceTypes


class WorldInterfaceMetadata(BaseModel):
    key: str
    name: str
    description: str
    type: WorldInstanceTypes = WorldInstanceTypes.SYNCHRONOUS
    status: str = "unavailable"  # "available", "unavailable", "deprecated"

    gravity: float = 5.0
    friction: float = 1.0
    allow_multi_direction: bool = True
    evaporation_coefficient: float = 8.0
    heat_to_water_evaporation: float = 120.0
    water_minimum_units: int = 30000
    metabolism_cost_to_apply_force: float = 1.999999949504854e-06

    host: str = "localhost"
    port: str = "8765"
