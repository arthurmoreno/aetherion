from __future__ import annotations

from typing import TypedDict


class BlockDimensions(TypedDict):
    width: int
    height: int
    depth: int


class PlayerBlockPosition(TypedDict):
    x: int
    y: int


class ResolutionConfig(TypedDict):
    BLOCKS_IN_SCREEN: BlockDimensions
    SPRITE_SCALE: int
    RIGHT_OFFSET: int
    UP_OFFSET: int
    GAME_SCREEN_WIDHT: int
    GAME_SCREEN_HEIGHT: int
    CAMERA_SCREEN_WIDTH_ADJUST_OFFSET: int
    CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET: int


class ScreenParameters(TypedDict):
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
