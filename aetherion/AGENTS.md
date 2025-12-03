# Agent Instructions - Aetherion

This document provides specific guidelines for working on the `aetherion` project (C++/Python Game Engine).

## Domain Overview

*   **Focus**: Performance-critical components, Rendering, Physics, Core Engine Architecture.
*   **Role**: Provides the foundation and bindings for `lifesim`.
*   **Key Components**:
    *   `src/`: C++ source code.
    *   `include/`: C++ header files.
    *   `bindings/`: Python bindings (nanobind/pybind11).

## Code Style & Standards

*   **Language**: C++17 / Python (Bindings)
*   **Formatter**: `clang-format` (Google Style).
*   **Naming Conventions**:
    *   **C++ (Core Engine - `src/`, `include/`):**
        *   Classes, Structs, Enums: `PascalCase`
        *   Public Methods: `PascalCase`
        *   Free Functions, Accessors, Mutators: `snake_case`
        *   Variables: `snake_case`
        *   Constants: `UPPER_CASE`
    *   **C++ (Python Bindings - `bindings/`):**
        *   Internal C++ implementation: Follow **C++ (Core Engine)** conventions.
        *   Exposed Python names (via `nanobind`/`pybind11`): Follow **Python (PEP 8)** conventions.
    *   **Python (Scripts, Bindings Usage):**
        *   Classes: `PascalCase`
        *   Functions, Variables: `snake_case`
        *   Constants: `UPPER_CASE`
    *   Members: `m_variableName` (or consistent with existing codebase if different).

### Example
```cpp
class PhysicsSystem {
public:
    void update(float delta_time);
private:
    float m_gravity;
};
```

## Workflow & Commands

*   **Environment**: `aetherion-312`
    *   **CRITICAL**: You MUST switch to this environment for engine work.
    ```bash
    conda deactivate
    conda activate aetherion-312
    ```

*   **Build & Install**:
    ```bash
    python -m build
    ```

*   **Dependencies**:
    *   Managed via Conda and system packages.

## Agent Tips

1.  **Performance is Key**: This is the engine. Optimize for speed and memory efficiency.
2.  **Memory Management**: Be vigilant about memory leaks. Use smart pointers (`std::shared_ptr`, `std::unique_ptr`) where possible.
3.  **Bindings**: Keep Python bindings clean and minimal. Expose only what is necessary for `lifesim`.
4.  **Safety**: Ensure C++ exceptions are caught and translated to Python exceptions in bindings.
