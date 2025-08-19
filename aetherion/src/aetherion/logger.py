"""
Simple logging utilities for the Aetherion Python package.

Goals
- Play nicely when LifeSim (or any host app) already configures logging.
- Offer a minimal standalone configuration for development/production.
- Optionally bridge stdlib logs to the C++ logger (aetherion.Logger) in cpp-dev.

Usage in library modules
    import logging
    logger = logging.getLogger(__name__)
    logger.info("Hello from Aetherion")

Applications can call configure_logging() once at startup, or, if using LifeSim,
import lifesim.logger.configure_logging which will also apply to Aetherion modules.
"""

from __future__ import annotations

import logging
import os
from collections.abc import Mapping
from typing import Protocol, cast, override, runtime_checkable

import structlog
from structlog.stdlib import BoundLogger

# Detect environment: default to production if not set
ENV: str = os.getenv("ENV", "production")


# --- Formatting helpers (compact color output) ---
RESET = "\033[0m"
CYAN = "\033[36m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
RED = "\033[31m"
WHITE = "\033[37m"


def production_renderer(_logger: BoundLogger, _method_name: str, event_dict: Mapping[str, object]) -> str:
    timestamp = str(event_dict.get("timestamp", ""))
    log_level: str = str(event_dict.get("log_level", ""))
    level = log_level.upper()
    event = str(event_dict.get("event", ""))

    if level == "INFO":
        level_color = GREEN
    elif level == "WARNING":
        level_color = YELLOW
    elif level == "ERROR":
        level_color = RED
    else:
        level_color = WHITE

    extra = {k: v for k, v in event_dict.items() if k not in ("timestamp", "log_level", "event")}
    extra_str = " ".join(f"{k}={v}" for k, v in extra.items())

    formatted = f"{CYAN}[{timestamp}]{RESET} {level_color}{level}:{RESET} {event}"
    if extra_str:
        formatted += f" {extra_str}"
    return formatted


# --- C++ logger protocol (typed interface) ---
@runtime_checkable
class _CppLoggerProtocol(Protocol):
    @staticmethod
    def initialize_spdlog(*, logger_name: str, log_file: str, console_output: bool) -> None: ...

    @staticmethod
    def set_level(level: int) -> None: ...

    @staticmethod
    def critical(msg: str) -> None: ...

    @staticmethod
    def error(msg: str) -> None: ...

    @staticmethod
    def warn(msg: str) -> None: ...

    @staticmethod
    def info(msg: str) -> None: ...

    @staticmethod
    def debug(msg: str) -> None: ...


# --- Bridge handler to C++ logger (optional) ---
class _CppLoggerHandler(logging.Handler):
    """Forward stdlib logging records to aetherion.Logger (C++), if available."""

    _initialized: bool = False

    def __init__(self, logger_name: str = "aetherion", log_file: str = "aetherion.log") -> None:
        super().__init__()
        self.logger_name: str = logger_name
        self.log_file: str = log_file

    def _ensure_cpp_logger(self) -> None:
        if self.__class__._initialized:
            return
        try:
            import aetherion  # type: ignore

            logger_obj = cast(object, aetherion.Logger)  # type: ignore[attr-defined]
            logger_cls = cast(_CppLoggerProtocol, logger_obj)
            logger_cls.initialize_spdlog(
                logger_name=self.logger_name,
                log_file=self.log_file,
                console_output=True,
            )
            level = getattr(aetherion, "LogLevel_DEBUG", None)
            if isinstance(level, int):
                logger_cls.set_level(level)
            self.__class__._initialized = True
        except Exception:
            self.__class__._initialized = False

    @override
    def emit(self, record: logging.LogRecord) -> None:  # pragma: no cover
        try:
            import aetherion  # type: ignore
        except Exception:
            return
        self._ensure_cpp_logger()
        try:
            msg = self.format(record)
            lvl = record.levelno
            logger_obj = cast(object, aetherion.Logger)  # type: ignore[attr-defined]
            logger_cls = cast(_CppLoggerProtocol, logger_obj)
            if lvl >= logging.CRITICAL and hasattr(cast(object, logger_cls), "critical"):
                logger_cls.critical(msg)
            elif lvl >= logging.ERROR and hasattr(cast(object, logger_cls), "error"):
                logger_cls.error(msg)
            elif lvl >= logging.WARNING and hasattr(cast(object, logger_cls), "warn"):
                logger_cls.warn(msg)
            elif lvl >= logging.INFO and hasattr(cast(object, logger_cls), "info"):
                logger_cls.info(msg)
            else:
                if hasattr(cast(object, logger_cls), "debug"):
                    logger_cls.debug(msg)
        except Exception:
            pass


def _inherit_from_lifesim(
    _env: str | None, _app_logger_name: str, _level: int | None, _use_cpp_bridge: bool | None
) -> BoundLogger | None:
    """Deprecated no-op: LifeSim logger inheritance removed.

    Always returns None so this module configures logging independently.
    """
    return None


def configure_logging(
    env: str | None = None,
    *,
    app_logger_name: str = "aetherion",
    level: int | None = None,
    use_cpp_bridge: bool | None = None,
    inherit: bool = True,
) -> BoundLogger:
    """Configure logging for Aetherion.

    - If inherit=True and LifeSim's configure_logging is present, delegate to it.
    - Otherwise, apply a minimal structlog+stdlib setup (dev/prod) and optionally attach
      a bridge to the C++ logger when env=="cpp-dev" or use_cpp_bridge=True.
    """

    current_env = env or ENV

    if inherit:
        delegated = _inherit_from_lifesim(current_env, app_logger_name, level, use_cpp_bridge)
        if delegated is not None:
            return delegated

    base_level = level
    if base_level is None:
        base_level = logging.DEBUG if current_env == "development" else logging.INFO

    # Start clean to avoid duplicate handlers in REPL/dev cycles
    root = logging.getLogger()
    for h in list(root.handlers):
        root.removeHandler(h)

    logging.basicConfig(format="%(message)s", level=base_level, force=True)

    if current_env == "development":
        processors = [
            structlog.stdlib.add_log_level,
            structlog.stdlib.PositionalArgumentsFormatter(),
            structlog.processors.TimeStamper(fmt="iso"),
            structlog.dev.ConsoleRenderer(),
        ]
    else:
        processors = [
            structlog.stdlib.add_log_level,
            structlog.stdlib.PositionalArgumentsFormatter(),
            structlog.processors.TimeStamper(fmt="iso"),
            production_renderer,
        ]

    structlog.configure(
        processors=processors,
        context_class=dict,
        logger_factory=structlog.stdlib.LoggerFactory(),
        wrapper_class=structlog.stdlib.BoundLogger,
        cache_logger_on_first_use=True,
    )

    if (use_cpp_bridge is True) or (use_cpp_bridge is None and current_env == "cpp-dev"):
        cpp_handler = _CppLoggerHandler(logger_name=app_logger_name, log_file=f"{app_logger_name}.log")
        cpp_handler.setLevel(base_level)
        root.addHandler(cpp_handler)

    app_logger: BoundLogger = cast(BoundLogger, structlog.get_logger(app_logger_name))
    app_logger.info("Aetherion logger configured", environment=current_env)
    return app_logger


def get_logger(name: str | None = None) -> BoundLogger:
    """Return a structlog BoundLogger for Aetherion or consumers."""
    return cast(BoundLogger, structlog.get_logger(name or "aetherion"))


# Configure on import with defaults (safe in both app/lib contexts)
logger = configure_logging(inherit=True)
