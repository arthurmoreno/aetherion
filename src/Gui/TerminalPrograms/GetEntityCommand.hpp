#pragma once

#include <string>
#include <vector>

#include "TerminalCommand.hpp"

/**
 * @brief GetEntity command - Queries entity info at a given world position
 *
 * Usage: get_entity <x> <y> <z>
 * Dispatches a query_get_entity request to the simulation and the result
 * is forwarded back to the terminal via context.consoleLogs.
 */
class GetEntityCommand : public TerminalCommand {
public:
  void execute(GuiContext &context, std::deque<TerminalLine> &terminalBuffer,
               bool &scrollToBottom) override;

  void setTokens(const std::vector<std::string> &tokens) override {
    tokens_ = tokens;
  }

  std::string getName() const override { return "get_entity"; }

  std::string getDescription() const override {
    return "Get entity info at position: get_entity <x> <y> <z>";
  }

private:
  std::vector<std::string> tokens_;
};
