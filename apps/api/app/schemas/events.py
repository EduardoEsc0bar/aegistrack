from __future__ import annotations

from datetime import datetime
from typing import Any

from app.schemas.common import ApiModel


class EventSummary(ApiModel):
    id: str
    asset_id: str | None
    type: str
    severity: str
    title: str
    description: str
    occurred_at: datetime
    source: str
    metadata: dict[str, Any]


class AlertSummary(ApiModel):
    id: str
    event_id: str | None
    status: str
    rule_name: str
    message: str
    severity: str
    created_at: datetime
    metadata: dict[str, Any]
