#pragma once

#include <memory>
#include <string>

#include "../../components/core/GuiContext.hpp"
#include "BasicProgram.hpp"

/**
 * @brief Base class for GUI programs in the GUI OS
 *
 * Extends BasicProgram with rendering capabilities. Each GUI program represents
 * an independent window/feature that can be shown/hidden and rendered independently.
 * Programs are analogous to OS processes - they have lifecycle management, state,
 * and can communicate via the command queue.
 *
 * Design Philosophy:
 * - Each program is self-contained and owns its rendering logic
 * - Programs communicate via commands (loose coupling)
 * - Programs can be activated/deactivated independently
 * - The GuiProgramManager acts as the "process scheduler"
 */
class GuiProgram : public BasicProgram {
   public:
    virtual ~GuiProgram() = default;

    /**
     * @brief Render the program's GUI
     *
     * Called once per frame if the program is active. The program should render
     * its ImGui window(s) and append any generated commands to context.commands.
     *
     * @param context Shared context containing all GUI data
     */
    virtual void render(GuiContext& context) = 0;
};
