from __future__ import annotations

from datetime import datetime
from typing import Any

from app.schemas.common import ApiModel


class HealthResponse(ApiModel):
    status: str
    service: str
    timestamp: datetime
    profile: dict[str, Any]
    bridge: dict[str, Any]
