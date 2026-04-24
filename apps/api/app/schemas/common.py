from __future__ import annotations

from datetime import datetime
from typing import Any

from pydantic import BaseModel, ConfigDict, Field


class ApiModel(BaseModel):
    model_config = ConfigDict(from_attributes=True)


class Position(ApiModel):
    lat: float
    lon: float
    alt: float


class Velocity(ApiModel):
    heading_deg: float
    speed_mps: float


class FileSource(ApiModel):
    source: str
    updated_at: datetime | None = None


class MetadataEnvelope(ApiModel):
    metadata: dict[str, Any] = Field(default_factory=dict)
