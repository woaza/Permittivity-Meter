from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class HardwareTestConfig:
    port: str | None
    baud: int
    serial_timeout: float
    cmd_delay: float
    stage_timeout: float
    reset_wait: float
