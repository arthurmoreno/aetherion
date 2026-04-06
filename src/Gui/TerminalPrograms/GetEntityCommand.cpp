#include "GetEntityCommand.hpp"

#include <nanobind/nanobind.h>

#include <stdexcept>
#include <string>

#include "../../components/core/GuiContext.hpp"

namespace nb = nanobind;

void GetEntityCommand::execute(GuiContext &context,
                               std::deque<TerminalLine> &terminalBuffer,
                               bool &scrollToBottom) {
  if (tokens_.size() < 3) {
    addOutput(terminalBuffer,
              "Usage: get_entity <x> <y> <z>  (e.g. get_entity 5 50 8)", false,
              true);
    scrollToBottom = true;
    return;
  }

  int x = 0;
  int y = 0;
  int z = 0;
  try {
    x = std::stoi(tokens_[0]);
    y = std::stoi(tokens_[1]);
    z = std::stoi(tokens_[2]);
  } catch (const std::exception &) {
    addOutput(terminalBuffer, "Error: x, y, z must be integers", false, true);
    scrollToBottom = true;
    return;
  }

  nb::dict params;
  params[nb::str("x")] = nb::int_(x);
  params[nb::str("y")] = nb::int_(y);
  params[nb::str("z")] = nb::int_(z);

  nb::dict command;
  command[nb::str("type")] = nb::str("query_get_entity");
  command[nb::str("params")] = params;

  context.commands.append(command);

  addOutput(terminalBuffer,
            "Querying entity at (" + std::to_string(x) + ", " +
                std::to_string(y) + ", " + std::to_string(z) + ")...",
            false, false);
  scrollToBottom = true;
}
