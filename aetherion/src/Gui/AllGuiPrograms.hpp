#pragma once

/**
 * @file AllGuiPrograms.hpp
 * @brief Convenience header that includes all GUI programs
 * 
 * This header provides a single point of inclusion for all GUI programs,
 * making it easy to register them in the program manager.
 */

// Core infrastructure
#include "GuiCore/GuiProgram.hpp"
#include "GuiCore/GuiContext.hpp"
#include "GuiCore/GuiProgramManager.hpp"

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

// Game programs
#include "GuiPrograms/Game/InventoryProgram.hpp"
#include "GuiPrograms/Game/EquipmentProgram.hpp"
#include "GuiPrograms/Game/PlayerStatsProgram.hpp"
