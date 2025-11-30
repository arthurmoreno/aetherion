# GUI OS Refactoring - Complete Implementation Guide

## 📋 Overview

This document describes the **GUI Operating System** refactoring that transformed the monolithic `renderInGameGuiFrame` function into a modular, extensible program-based architecture.

## 🎯 Goals Achieved

✅ **Separation of Concerns**: Each GUI window is now a self-contained "program"  
✅ **Scalability**: Adding new windows requires only creating a new `GuiProgram` subclass  
✅ **Maintainability**: Main orchestration function reduced from ~170 lines to ~60 lines  
✅ **Clear Architecture**: Programs communicate via command queue (loose coupling)  
✅ **State Management**: Centralized program lifecycle management via `GuiProgramManager`

---

## 📁 New Directory Structure

```
src/Gui/
├── GuiCore/                          # Core infrastructure
│   ├── GuiContext.hpp               # Shared context for all programs
│   ├── GuiProgram.hpp               # Base class for all GUI programs
│   └── GuiProgramManager.hpp       # Singleton managing program lifecycle
│
├── GuiPrograms/                     # All GUI program implementations
│   ├── Settings/                    # Settings-related programs
│   │   ├── SettingsProgram.hpp     # Main settings launcher
│   │   ├── PhysicsSettingsProgram.hpp
│   │   └── CameraSettingsProgram.hpp
│   │
│   ├── Game/                        # Game-related programs
│   │   ├── InventoryProgram.hpp
│   │   ├── EquipmentProgram.hpp
│   │   └── PlayerStatsProgram.hpp
│   │
│   └── Debug/                       # Debug & diagnostic programs
│       ├── GeneralMetricsProgram.hpp
│       ├── EntitiesStatsProgram.hpp
│       ├── GadgetsProgram.hpp
│       ├── EntityInterfaceProgram.hpp
│       └── AIStatisticsProgram.hpp
│
├── AllGuiPrograms.hpp               # Convenience include for all programs
├── Gui.cpp                          # Main GUI implementation
├── Gui.hpp                          # GUI public interface
└── README_GUI_REFACTOR.md          # This file
```

---

## 🏗️ Architecture Components

### 1. **GuiContext** (`GuiCore/GuiContext.hpp`)

**Purpose**: Dependency injection container for all GUI programs

**Contains**:
- World state (ticks, FPS, world pointer)
- Python data bindings (physics, inventory, console logs, commands)
- Entity interfaces (generic, hovered, selected)

**Benefits**:
- Single source of truth for GUI data
- No need to pass 13 parameters to each program
- Easy to extend with new data fields

### 2. **GuiProgram** (`GuiCore/GuiProgram.hpp`)

**Purpose**: Abstract base class for all GUI programs

**Key Methods**:
```cpp
virtual void render(GuiContext& context) = 0;  // Render program GUI
virtual std::string getId() const = 0;         // Unique program ID
virtual std::string getDisplayName() const = 0; // Human-readable name
virtual bool isActive() const;                  // Check if active
virtual void setActive(bool active);            // Activate/deactivate
virtual void onActivate();                      // Lifecycle hook
virtual void onDeactivate();                    // Lifecycle hook
```

**Benefits**:
- Enforces consistent interface for all programs
- Supports lifecycle management
- Easy to test individual programs

### 3. **GuiProgramManager** (`GuiCore/GuiProgramManager.hpp`)

**Purpose**: Singleton managing all GUI program instances

**Key Methods**:
```cpp
void registerProgram(std::shared_ptr<GuiProgram> program);
bool activateProgram(const std::string& programId);
bool deactivateProgram(const std::string& programId);
bool toggleProgram(const std::string& programId);
void renderAllPrograms(GuiContext& context);
std::vector<std::string> getAllProgramIds() const;
bool isProgramActive(const std::string& programId) const;
```

**Benefits**:
- Centralized program registry
- Easy program activation/deactivation
- Single point for rendering all active programs

---

## 🔄 Migration from Old to New System

### Before (Monolithic):
```cpp
void renderInGameGuiFrame(...13 parameters...) {
    // Frame setup
    ImGui::NewFrame();
    
    // ~170 lines of if-statements for each window
    if (showSettings) {
        ImGui::Begin("Settings", &showSettings);
        // Render settings...
        ImGui::End();
    }
    
    if (showPhysicsSettings) {
        ImGui::Begin("Physics Settings", &showPhysicsSettings);
        // Render physics settings...
        ImGui::End();
    }
    
    // ... 10+ more windows ...
}
```

### After (Program-Based):
```cpp
void renderInGameGuiFrame(...13 parameters...) {
    // Frame setup
    ImGui::NewFrame();
    
    // Build context
    GuiContext context{...};
    
    // Process program activation commands
    // (handle "activate_program" commands)
    
    // Render all active programs (GUI OS)
    GuiProgramManager::Instance()->renderAllPrograms(context);
    
    // Render always-on components
    RenderConsoleWindow(...);
    HandleDragDropToWorld(...);
}
```

---

## 📝 How to Add a New GUI Program

### Step 1: Create Program Class

Create a new file in the appropriate directory:
- `GuiPrograms/Settings/` for settings-related programs
- `GuiPrograms/Game/` for gameplay-related programs
- `GuiPrograms/Debug/` for debug/diagnostic programs

**Example** (`GuiPrograms/Game/CraftingProgram.hpp`):
```cpp
#pragma once

#include "../../GuiCore/GuiProgram.hpp"
#include <imgui.h>

class CraftingProgram : public GuiProgram {
public:
    void render(GuiContext& context) override {
        if (!isActive_) return;
        
        if (ImGui::Begin("Crafting", &isActive_)) {
            // Your crafting UI code here
            ImGui::Text("Crafting Interface");
            // Access context data as needed
            // Append commands to context.commands
        }
        ImGui::End();
    }
    
    std::string getId() const override { 
        return "crafting"; 
    }
    
    std::string getDisplayName() const override { 
        return "Crafting"; 
    }
};
```

### Step 2: Register Program

Add to `initializeGuiPrograms()` in `Gui.cpp`:
```cpp
void initializeGuiPrograms() {
    auto* manager = GuiProgramManager::Instance();
    
    // ... existing registrations ...
    
    // Register your new program
    manager->registerProgram(std::make_shared<CraftingProgram>());
}
```

### Step 3: Add to AllGuiPrograms.hpp

Include your program header:
```cpp
// Game programs
#include "GuiPrograms/Game/InventoryProgram.hpp"
#include "GuiPrograms/Game/EquipmentProgram.hpp"
#include "GuiPrograms/Game/CraftingProgram.hpp"  // Add this
```

### Step 4: Add UI Button (Optional)

To add a button in the top bar (`RenderTopBar()` in `Gui.cpp`):
```cpp
if (ImGui::Button("Crafting")) {
    GuiProgramManager::Instance()->toggleProgram("crafting");
}
```

**That's it!** Your new program is fully integrated. 🎉

---

## 🔗 Inter-Program Communication

Programs communicate via the **command queue** pattern:

### Activating Another Program:
```cpp
void render(GuiContext& context) override {
    if (ImGui::Button("Open Crafting")) {
        nb::dict cmd;
        cmd["type"] = "activate_program";
        cmd["program_id"] = "crafting";
        context.commands.append(cmd);
    }
}
```

### Custom Commands:
```cpp
void render(GuiContext& context) override {
    if (ImGui::Button("Drop Item")) {
        nb::dict cmd;
        cmd["type"] = "drop_item";
        cmd["item_id"] = 123;
        cmd["quantity"] = 5;
        context.commands.append(cmd);
    }
}
```

Commands are processed by the Python layer after the frame is rendered.

---

## 🎨 Benefits of This Architecture

### 1. **Separation of Concerns**
Each program is self-contained with its own:
- State management
- Rendering logic
- Command generation

### 2. **Testability**
Programs can be tested in isolation:
```cpp
// Unit test example
auto program = std::make_shared<InventoryProgram>();
GuiContext testContext{...};
program->render(testContext);
// Assert expected behavior
```

### 3. **Extensibility**
Adding features is trivial:
- New program? Create a class + register it
- New lifecycle event? Add to `GuiProgram` base class
- New shared data? Add field to `GuiContext`

### 4. **Maintainability**
- Main function is < 60 lines (was 170+)
- Each program file is focused and < 100 lines
- Clear dependencies via `GuiContext`

### 5. **Performance**
- Only active programs are rendered
- No performance overhead (programs are checked with simple boolean flag)
- Manager uses `std::unordered_map` for O(1) program lookup

---

## 🐛 Troubleshooting

### Program Not Showing
1. Check if program is registered in `initializeGuiPrograms()`
2. Verify program ID matches activation command
3. Ensure `isActive_` is being set correctly

### Include Errors
All programs should use these include patterns:
- `#include "../../GuiCore/GuiProgram.hpp"` (always)
- `#include "Gui/GuiStateManager.hpp"` (if needed)
- `#include "World.hpp"` (if needed, not `../World.hpp`)

### Program Not Receiving Context Data
Ensure `GuiContext` is constructed correctly in `renderInGameGuiFrame()` and all fields are populated.

---

## 📚 Further Improvements (Future)

### Short-term:
- [ ] Add program dependencies (e.g., "inventory requires crafting")
- [ ] Add program priority for rendering order
- [ ] Implement program state persistence

### Medium-term:
- [ ] Create visual program manager UI (task manager style)
- [ ] Add program resource monitoring (memory, CPU)
- [ ] Implement program crash recovery

### Long-term:
- [ ] Hot-reload programs at runtime
- [ ] Plugin system for external programs
- [ ] Multi-workspace support (multiple game instances)

---

## 👥 Credits

**Architecture Design**: Based on OS process management patterns  
**Implementation**: GUI OS refactoring (November 2025)  
**Inspiration**: ImGui examples, game engine UI systems

---

## 📄 License

Same as main project license.

---

**Questions?** See the code comments in each file for detailed explanations.
