from __future__ import annotations

from datetime import datetime
from typing import Any

from app.schemas.common import ApiModel


class AssetSummary(ApiModel):
    id: str
    name: str
    type: str
    status: str
    latitude: float
    longitude: float
    altitude: float
    heading: float
    speed: float
    metadata: dict[str, Any]
    created_at: datetime
    updated_at: datetime


class SensorSummary(ApiModel):
    id: str
    name: str
    type: str
    latitude: float
    longitude: float
    range_km: float
    azimuth_deg: float
    elevation_deg: float
    is_active: bool
    metadata: dict[str, Any]


class TrackSummary(ApiModel):
    id: str
    asset_id: str
    classification: str
    confidence: float
    status: str
    predicted_path: list[dict[str, float]]
    last_update_at: datetime
