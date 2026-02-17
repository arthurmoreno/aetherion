# Terminal Programs Architecture

## Class Hierarchy

```
TerminalCommand (abstract base class)
│
├── ClearCommand
├── HelpCommand
├── HistoryCommand
└── QueueCommand
```

## Component Interaction

```
┌─────────────────────────────────────────────────────────────┐
│                     ConsoleProgram                          │
│  (Manages UI, input handling, and terminal display)        │
│                                                             │
│  ┌───────────────────────────────────────────────────┐    │
│  │ Command Registry (std::map)                       │    │
│  │  - "clear"   -> ClearCommand instance            │    │
│  │  - "help"    -> HelpCommand instance             │    │
│  │  - "history" -> HistoryCommand instance          │    │
│  │  - "queue"   -> QueueCommand instance            │    │
│  └───────────────────────────────────────────────────┘    │
│                                                             │
│  When user types command:                                  │
│  1. Parse input                                            │
│  2. Lookup in registry                                     │
│  3. Call command->execute(context, buffer, scroll)         │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ delegates to
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   TerminalCommand                           │
│                 (Abstract Base Class)                       │
│                                                             │
│  + execute(context, buffer, scroll) : void [virtual]       │
│  + getName() : string [virtual]                            │
│  + getDescription() : string [virtual]                     │
│  # addOutput(buffer, text, isCmd, isErr) : void [protected]│
└─────────────────────────────────────────────────────────────┘
                              │
                              │ inherited by
              ┌───────────────┼───────────────┬──────────────┐
              ▼               ▼               ▼              ▼
       ┌──────────┐    ┌──────────┐   ┌──────────┐   ┌──────────┐
       │  Clear   │    │   Help   │   │ History  │   │  Queue   │
       │ Command  │    │ Command  │   │ Command  │   │ Command  │
       └──────────┘    └──────────┘   └──────────┘   └──────────┘
```

## Data Flow

```
User Input
    │
    ▼
ConsoleProgram::executeCommand()
    │
    ├─> Parse command string
    │
    ├─> Add to history
    │
    ├─> Look up in commands_ map
    │
    └─> If found:
            │
            ▼
        TerminalCommand::execute()
            │
            ├─> Perform command-specific logic
            │
            └─> addOutput() to terminal buffer
                    │
                    ▼
                Terminal buffer updated
                    │
                    ▼
                ConsoleProgram renders updated buffer
```

## File Dependencies

```
ConsoleProgram.hpp
    │
    ├─> includes TerminalCommand.hpp
    ├─> includes ClearCommand.hpp
    ├─> includes HelpCommand.hpp
    ├─> includes HistoryCommand.hpp
    └─> includes QueueCommand.hpp

TerminalCommand.hpp
    │
    └─> Forward declares: TerminalLine, GuiContext

Each Command.hpp
    │
    ├─> includes TerminalCommand.hpp
    └─> potentially includes GuiStateManager.hpp (for context)
```

## Memory Management

All commands are managed using `std::shared_ptr`:
- Created in `ConsoleProgram::initializeCommands()`
- Stored in both:
  1. `std::map<string, shared_ptr<TerminalCommand>>` - for fast lookup
  2. `std::vector<shared_ptr<TerminalCommand>>` - for ordered iteration
- Automatically destroyed when ConsoleProgram is destroyed

## Thread Safety

Current implementation is **not thread-safe** (assumes single-threaded GUI context):
- Terminal buffer access is not synchronized
- Command execution is synchronous
- All operations happen on the main/GUI thread

If multi-threading is needed in the future:
- Add mutex protection for terminal buffer
- Consider making command execution async
- Use thread-safe command queue
