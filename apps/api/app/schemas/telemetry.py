from __future__ import annotations

from datetime import datetime

from app.schemas.common import ApiModel, Position, Velocity


class TelemetryTrack(ApiModel):
    confidence: float
    classification: str


class TelemetryUpdate(ApiModel):
    type: str
    timestamp: datetime
    asset_id: str
    position: Position
    velocity: Velocity
    track: TelemetryTrack
    alerts: list[str]
    source: str
