from __future__ import annotations

import math
from typing import Any


def repartition_animation(array: list[int], size: int) -> list[int]:
    ratio = math.ceil(size / len(array))
    animation_distributed: list[int] = []
    array_index = 0
    for i in range(1, size + 1):
        animation_distributed.append(array[array_index])
        if i != 0 and i % ratio == 0:
            array_index += 1

    return animation_distributed


def calc_pixel_offset_linear(completion_time: int, time_remaining: int, tile_size_on_screen: int) -> int:
    return math.floor(tile_size_on_screen - (tile_size_on_screen * (time_remaining / completion_time)))


def calc_pixel_offset_inverted_quadratic(completion_time: int, time_remaining: int, tile_size_on_screen: int) -> int:
    """Calculate the pixel offset with quadratic easing.

    :param completion_time: Total time steps to complete the movement.
    :param time_remaining: Time steps remaining.
    :return: The calculated pixel offset.
    """
    # Simulating gravity effect using a quadratic easing-out pattern
    progress = (completion_time - time_remaining) / completion_time  # progress is between 0 and 1
    eased_progress = 1 - (1 - progress) ** 3  # Quadratic easing-out function

    return math.floor(tile_size_on_screen * eased_progress)


def calc_pixel_offset_quadratic(completion_time: int, time_remaining: int, tile_size_on_screen: int) -> int:
    """Calculate the pixel offset with smooth quadratic easing."""
    progress = (completion_time - time_remaining) / completion_time  # progress is between 0 and 1
    eased_progress = progress**3  # Smooth quadratic increase

    return math.floor(tile_size_on_screen * eased_progress)


def calc_pixel_offset_logarithmic(completion_time: int, time_remaining: int, tile_size_on_screen: int) -> int:
    """Logarithmic easing-out function for pixel offset.

    :param completion_time: Total time steps to complete the movement.
    :param time_remaining: Time steps remaining.
    :return: The calculated pixel offset.
    """
    # Avoid log(0) by ensuring progress is within [0, 1]
    progress = (completion_time - time_remaining) / completion_time

    # Apply logarithmic easing: log(1 + progress) normalized by log(2)
    if progress == 0:
        return 0
    else:
        log_progress = math.log(1 + progress) / math.log(2)
        return math.floor(tile_size_on_screen * log_progress)


def entity_animation_handler(
    event: Any,
    entity: Any,
    entity_view: Any,
    screen_x: int,
    screen_y: int,
    tile_size_on_screen: int,
) -> tuple[int, int]:
    moving_component = entity.get_moving_component()

    completion_time = moving_component.completion_time
    time_remaining = moving_component.time_remaining
    direction = moving_component.direction

    diff_x = entity.get_position().x - moving_component.moving_from_x
    diff_y = entity.get_position().y - moving_component.moving_from_y
    diff_z = entity.get_position().z - moving_component.moving_from_z

    screen_x -= diff_x * tile_size_on_screen
    screen_y -= diff_y * tile_size_on_screen
    if diff_z:
        screen_x += diff_z * tile_size_on_screen
        screen_y += diff_z * tile_size_on_screen

    animation_index = completion_time - time_remaining - 1

    # If the entity is going up or down uses the current entity direction
    if direction.value == 5 or direction.value == 6:
        entity_direction = entity.get_position().direction.value
    else:
        entity_direction = direction.value

    entity_view.animation_sprites[entity_direction]
    possible_steps = list(entity_view.animation_sprites[entity_direction].keys())
    animation_distributed = repartition_animation(possible_steps, completion_time)
    entity_view.set_animation_sprite(animation_distributed[animation_index], entity_direction)

    # Adjust the entity direction based on the velocity
    velocity_x = moving_component.vx
    velocity_y = moving_component.vy
    velocity_z = moving_component.vz

    pixel_offset = calc_pixel_offset_linear(completion_time, time_remaining, tile_size_on_screen)

    if velocity_z > 0 and moving_component.will_stop_z:
        pixel_offset_z = calc_pixel_offset_inverted_quadratic(completion_time, time_remaining, tile_size_on_screen)
    elif velocity_z < 0 and moving_component.will_stop_z:
        pixel_offset_z = calc_pixel_offset_quadratic(completion_time, time_remaining, tile_size_on_screen)
    else:
        pixel_offset_z = calc_pixel_offset_linear(completion_time, time_remaining, tile_size_on_screen)

    _screen_x = screen_x
    _screen_y = screen_y

    _screen_x += pixel_offset * (abs(diff_x) if velocity_x > 0 else -abs(diff_x) if velocity_x < 0 else 0)

    # Y-axis adjustment (up if velocity_y > 0, down if velocity_y < 0)
    _screen_y += pixel_offset * (abs(diff_y) if velocity_y > 0 else -abs(diff_y) if velocity_y < 0 else 0)

    # Z-axis adjustment — Combined with X and Y movements:
    if velocity_z != 0:
        z_factor_x = 1 if velocity_z < 0 else -1  # Adjust left or right for z
        z_factor_y = 1 if velocity_z < 0 else -1  # Adjust up or down for z

        # Apply the Z velocity effect in combination with existing X and Y adjustments
        _screen_x += (pixel_offset_z) * z_factor_x  # Adjust the x-axis based on z-movement
        _screen_y += (pixel_offset_z) * z_factor_y  # Adjust the y-axis based on z-movement

    return _screen_x, _screen_y
