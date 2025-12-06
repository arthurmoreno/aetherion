#pragma once

/**
 * @file AllGuiPrograms.hpp
 * @brief Convenience header that includes all GUI programs and program managers
 * 
 * This header provides a single point of inclusion for all GUI programs,
 * terminal programs, and their respective managers, making it easy to 
 * register them in the program managers.
 */

// Core infrastructure
#include "GuiCore/BasicProgram.hpp"
#include "GuiCore/GuiProgram.hpp"
#include "GuiCore/TerminalProgram.hpp"
#include "../components/core/GuiContext.hpp"

// Program managers
#include "GuiCore/ProgramManager.hpp"
#include "GuiCore/GuiProgramManager.hpp"
#include "GuiCore/TerminalProgramManager.hpp"

// Settings programs
#include "GuiPrograms/Settings/SettingsProgram.hpp"
#include "GuiPrograms/Settings/PhysicsSettingsProgram.hpp"
#include "GuiPrograms/Settings/CameraSettingsProgram.hpp"

// Debug programs
#include "GuiPrograms/Debug/GeneralMetricsProgram.hpp"
#include "GuiPrograms/Debug/EntitiesStatsProgram.hpp"
#include "GuiPrograms/Debug/GadgetsProgram.hpp"
#include "GuiPrograms/Debug/EntityInterfaceProgram.hpp"
#include "GuiPrograms/Debug/AIStatisticsProgram.hpp"
#include "GuiPrograms/Debug/ConsoleProgram.hpp"
#include "GuiPrograms/Debug/EditorDebuggerProgram.hpp"

// Game programs
#include "GuiPrograms/Game/InventoryProgram.hpp"
#include "GuiPrograms/Game/EquipmentProgram.hpp"
#include "GuiPrograms/Game/PlayerStatsProgram.hpp"
