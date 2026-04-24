from __future__ import annotations

from datetime import datetime

from pydantic import BaseModel

from app.schemas.common import ApiModel


class PlaybackState(ApiModel):
    id: str
    mode: str
    is_running: bool
    current_time: datetime | None
    start_time: datetime | None
    end_time: datetime | None
    source: str
    updated_at: datetime


class PlaybackStartRequest(BaseModel):
    start_time: datetime | None = None
    end_time: datetime | None = None


class PlaybackActionResponse(ApiModel):
    state: PlaybackState
