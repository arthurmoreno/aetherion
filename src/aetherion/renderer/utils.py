# Function to apply a lighting effect by adjusting sprite color
def apply_lighting_to_sprite(sprite, intensity):
    """Apply a lighting effect by adjusting the color of the sprite."""
    intensity = max(0.0, min(1.0, intensity))
    light_color = int(255 * intensity)
    sprite.color = (light_color, light_color, light_color)  # Apply grayscale lighting
