from __future__ import annotations

from pydantic import BaseModel


class BlockDimensions(BaseModel):
    width: int
    height: int
    depth: int


class PlayerBlockPosition(BaseModel):
    x: int
    y: int


class ResolutionConfig(BaseModel):
    BLOCKS_IN_SCREEN: BlockDimensions
    SPRITE_SCALE: int
    RIGHT_OFFSET: int
    UP_OFFSET: int
    GAME_SCREEN_WIDHT: int
    GAME_SCREEN_HEIGHT: int
    CAMERA_SCREEN_WIDTH_ADJUST_OFFSET: int
    CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET: int


class ScreenParameters(BaseModel):
    BLOCKS_IN_SCREEN: BlockDimensions
    PLAYER_BLOCK_POSITION: PlayerBlockPosition
    LAYERS_TO_DRAW: int
    LAYERS_BELLOW_PLAYER: int
    SPRITE_SIZE: int
    SPRITE_SCALE: int
    TILE_SIZE_ON_SCREEN: int
    RIGHT_OFFSET: int
    UP_OFFSET: int
    GAME_SCREEN_WIDHT: int
    GAME_SCREEN_HEIGHT: int
    CAMERA_SCREEN_WIDTH_ADJUST_OFFSET: int
    CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET: int
