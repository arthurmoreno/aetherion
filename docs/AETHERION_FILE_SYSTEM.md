# Aetherion File System Standard

This document defines the standard file system organization for projects built with the Aetherion engine. The goal is to strictly separate the **Engine Core**, **Game Assets/Logic**, and **User Runtime Data**, inspired by systems like Godot's `res://` protocol.

## 1. Path Protocols

To ensure portability and separation of concerns, the engine and game logic should never use absolute OS paths. Instead, they must use the following virtual protocols:

| Protocol | Description | Physical Mapping (Example) | Access |
| :--- | :--- | :--- | :--- |
| `res://` | **Resources**. The root of the game project. Contains all static assets, scripts, and configuration. | `/path/to/lifesim/` | Read-Only (mostly) |
| `user://` | **User Data**. Persistent storage for save games, cache, logs, and runtime generated content. | `/path/to/lifesim/.aetherion/` | Read/Write |
| `core://` | **Core Engine**. Built-in engine resources (default shaders, debug fonts, primitive meshes). | `/path/to/aetherion/data/` | Read-Only |

## 2. Game Project Structure (`res://`)

The `res://` root is the main entry point for any game built on Aetherion. It is defined by the presence of a `project.toml` file.

### Standard Directory Layout

```text
res://
├── project.toml          # Main configuration file (Entry Point)
├── .aetherion/           # (Mapped to user://) Runtime data, ignored by version control
│
├── ai/                   # High-level AI definitions
│   └── aievolution/      # Evolutionary algorithms and brain specs
│
├── audio/                # Sound effects and music
│
├── entities/             # Game entity definitions (Python classes/YAML)
│
├── scenes/               # Scene definitions (Layouts, Maps)
│
├── scripts/              # General purpose game scripts
│
├── sprites/              # Visual assets (Textures, SpriteSheets)
│
├── systems/              # ECS System implementations
│
└── world/                # World generation and terrain logic
```

### Folder Responsibilities

*   **`ai/`**: Contains the *logic* and *definitions* for AI (e.g., NEAT config, brain structures). It does **not** contain runtime populations or training checkpoints (see `user://`).
*   **`entities/`**: Definitions of game objects (e.g., `Player`, `Tree`).
*   **`systems/`**: Logic that runs every frame or tick (e.g., `MovementSystem`, `GrowthSystem`).
*   **`scripts/`**: Utility scripts or one-off logic that doesn't fit into the ECS system structure.
*   **`sprites/` & `audio/`**: Raw assets.

## 3. User Data Structure (`user://`)

The `user://` directory is for files generated *by* the game during execution. This folder should generally be added to `.gitignore`.

```text
user://
├── tmp/                  # Temporary files, cache, swap
├── logs/                 # Execution logs
├── saves/                # Save game files
├── screenshots/          # User captured images
└── aievolution/          # Runtime AI data (Checkpoints, Populations, Best Genomes)
```

## 4. Configuration (`project.toml`)

Every Aetherion project must have a `project.toml` at the root of `res://`. This file tells the engine how to load the game.

**Example `project.toml`:**

```toml
[project]
name = "Life Simulation"
version = "0.1.0"
main_scene = "res://scenes/main.scn"

[engine]
version = "3.1.2"

[paths]
# Optional overrides if non-standard structure is needed
# scripts = "res://src/scripts" 
```

## 5. Implementation Guidelines

### Path Resolution
The engine (C++ or Python adapter) must implement a path resolver:

```python
def resolve_path(virtual_path: str) -> Path:
    if virtual_path.startswith("res://"):
        return GAME_ROOT_DIR / virtual_path.replace("res://", "")
    elif virtual_path.startswith("user://"):
        return USER_DATA_DIR / virtual_path.replace("user://", "")
    elif virtual_path.startswith("core://"):
        return ENGINE_DATA_DIR / virtual_path.replace("core://", "")
    else:
        raise ValueError(f"Invalid path protocol: {virtual_path}")
```

### Asset Loading
Assets should always be referenced by their virtual path in code and data files:

```python
# Good
sprite = resource_manager.load("res://sprites/characters/player.png")

# Bad
sprite = resource_manager.load("/home/arthur/game/sprites/player.png")
```
