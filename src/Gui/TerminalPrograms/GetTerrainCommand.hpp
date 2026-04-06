#pragma once

#include <string>
#include <vector>

#include "TerminalCommand.hpp"

/**
 * @brief GetTerrain command - Queries terrain voxel info at a given world
 * position
 *
 * Usage: get_terrain <x> <y> <z>
 * Dispatches a query_get_terrain request to the simulation and the result
 * is forwarded back to the terminal via context.consoleLogs.
 */
class GetTerrainCommand : public TerminalCommand {
public:
  void execute(GuiContext &context, std::deque<TerminalLine> &terminalBuffer,
               bool &scrollToBottom) override;

  void setTokens(const std::vector<std::string> &tokens) override {
    tokens_ = tokens;
  }

  std::string getName() const override { return "get_terrain"; }

  std::string getDescription() const override {
    return "Get terrain info at position: get_terrain <x> <y> <z>";
  }

private:
  std::vector<std::string> tokens_;
};
