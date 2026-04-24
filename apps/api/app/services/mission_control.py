from __future__ import annotations

import json
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from sqlalchemy import select
from sqlalchemy.orm import Session

from app.core.config import get_settings
from app.db.models import Alert, Asset, Event, PlaybackSession, Sensor, TelemetryPoint, Track
from app.services.file_bridge import AegisTrackFileBridge


class MissionControlService:
    def __init__(self, db: Session) -> None:
        self.db = db
        self.settings = get_settings()
        self.bridge = AegisTrackFileBridge()

    def health(self) -> dict[str, Any]:
        profile: dict[str, Any] = {}
        profile_path = Path(self.settings.profile_path)
        if profile_path.exists():
            profile = json.loads(profile_path.read_text(encoding="utf-8"))
        snapshot = self.bridge.load_snapshot()
        return {
            "status": "ok",
            "service": self.settings.app_name,
            "timestamp": datetime.now(UTC),
            "profile": profile,
            "bridge": snapshot.bridge_status,
        }

    def list_assets(self) -> list[dict[str, Any]]:
        runtime_assets = {item["id"]: item for item in self.bridge.load_snapshot().assets}
        rows = self.db.scalars(select(Asset)).all()
        combined = [self._asset_to_dict(item) for item in rows]
        combined.extend(item for key, item in runtime_assets.items() if key not in {row["id"] for row in combined})
        return sorted(combined, key=lambda item: item["name"])

    def get_asset(self, asset_id: str) -> dict[str, Any] | None:
        for asset in self.list_assets():
            if asset["id"] == asset_id:
                return asset
        return None

    def list_sensors(self) -> list[dict[str, Any]]:
        runtime_sensors = {item["id"]: item for item in self.bridge.load_snapshot().sensors}
        rows = self.db.scalars(select(Sensor)).all()
        combined = [self._sensor_to_dict(item) for item in rows]
        combined.extend(item for key, item in runtime_sensors.items() if key not in {row["id"] for row in combined})
        return sorted(combined, key=lambda item: item["name"])

    def list_tracks(self) -> list[dict[str, Any]]:
        runtime_tracks = {item["id"]: item for item in self.bridge.load_snapshot().tracks}
        rows = self.db.scalars(select(Track)).all()
        combined = [self._track_to_dict(item) for item in rows]
        combined.extend(item for key, item in runtime_tracks.items() if key not in {row["id"] for row in combined})
        return sorted(combined, key=lambda item: item["last_update_at"], reverse=True)

    def list_events(self) -> list[dict[str, Any]]:
        runtime_events = {item["id"]: item for item in self.bridge.load_snapshot().events}
        rows = self.db.scalars(select(Event)).all()
        combined = [self._event_to_dict(item) for item in rows]
        combined.extend(item for key, item in runtime_events.items() if key not in {row["id"] for row in combined})
        return sorted(combined, key=lambda item: item["occurred_at"], reverse=True)

    def list_alerts(self) -> list[dict[str, Any]]:
        runtime_alerts = {item["id"]: item for item in self.bridge.load_snapshot().alerts}
        rows = self.db.scalars(select(Alert)).all()
        combined = [self._alert_to_dict(item) for item in rows]
        combined.extend(item for key, item in runtime_alerts.items() if key not in {row["id"] for row in combined})
        return sorted(combined, key=lambda item: item["created_at"], reverse=True)

    def latest_telemetry(self) -> list[dict[str, Any]]:
        runtime = self.bridge.load_snapshot().telemetry
        if runtime:
            return runtime
        rows = self.db.scalars(select(TelemetryPoint).order_by(TelemetryPoint.timestamp.desc())).all()
        return [self._telemetry_to_dict(row) for row in rows]

    def playback_state(self) -> dict[str, Any]:
        playback = self.db.get(PlaybackSession, "playback-primary")
        if playback is None:
            now = datetime.now(UTC)
            playback = PlaybackSession(
                id="playback-primary",
                mode="live",
                start_time=now,
                end_time=now,
                current_time=now,
                is_running=True,
                source="service",
                updated_at=now,
            )
            self.db.add(playback)
            self.db.commit()
            self.db.refresh(playback)
        return self._playback_to_dict(playback)

    def start_playback(self, start_time: datetime | None, end_time: datetime | None) -> dict[str, Any]:
        playback = self.db.get(PlaybackSession, "playback-primary")
        now = datetime.now(UTC)
        if playback is None:
            playback = PlaybackSession(
                id="playback-primary",
                mode="playback",
                start_time=start_time or now,
                end_time=end_time or now,
                current_time=start_time or now,
                is_running=True,
                source="mission-control",
                updated_at=now,
            )
            self.db.add(playback)
        else:
            playback.mode = "playback"
            playback.start_time = start_time or playback.start_time or now
            playback.end_time = end_time or playback.end_time or now
            playback.current_time = playback.start_time
            playback.is_running = True
            playback.updated_at = now
        self.db.commit()
        self.db.refresh(playback)
        return self._playback_to_dict(playback)

    def stop_playback(self) -> dict[str, Any]:
        playback = self.db.get(PlaybackSession, "playback-primary")
        now = datetime.now(UTC)
        if playback is None:
            return self.playback_state()
        playback.mode = "live"
        playback.is_running = False
        playback.current_time = now
        playback.updated_at = now
        self.db.commit()
        self.db.refresh(playback)
        return self._playback_to_dict(playback)

    def _asset_to_dict(self, row: Asset) -> dict[str, Any]:
        return {
            "id": row.id,
            "name": row.name,
            "type": row.type,
            "status": row.status,
            "latitude": row.latitude,
            "longitude": row.longitude,
            "altitude": row.altitude,
            "heading": row.heading,
            "speed": row.speed,
            "metadata": row.metadata_json,
            "created_at": row.created_at,
            "updated_at": row.updated_at,
        }

    def _sensor_to_dict(self, row: Sensor) -> dict[str, Any]:
        return {
            "id": row.id,
            "name": row.name,
            "type": row.type,
            "latitude": row.latitude,
            "longitude": row.longitude,
            "range_km": row.range_km,
            "azimuth_deg": row.azimuth_deg,
            "elevation_deg": row.elevation_deg,
            "is_active": row.is_active,
            "metadata": row.metadata_json,
        }

    def _track_to_dict(self, row: Track) -> dict[str, Any]:
        return {
            "id": row.id,
            "asset_id": row.asset_id,
            "classification": row.classification,
            "confidence": row.confidence,
            "status": row.status,
            "predicted_path": row.predicted_path,
            "last_update_at": row.last_update_at,
        }

    def _event_to_dict(self, row: Event) -> dict[str, Any]:
        return {
            "id": row.id,
            "asset_id": row.asset_id,
            "type": row.type,
            "severity": row.severity,
            "title": row.title,
            "description": row.description,
            "occurred_at": row.occurred_at,
            "source": row.source,
            "metadata": row.metadata_json,
        }

    def _alert_to_dict(self, row: Alert) -> dict[str, Any]:
        return {
            "id": row.id,
            "event_id": row.event_id,
            "status": row.status,
            "rule_name": row.rule_name,
            "message": row.message,
            "severity": row.severity,
            "created_at": row.created_at,
            "metadata": row.metadata_json,
        }

    def _telemetry_to_dict(self, row: TelemetryPoint) -> dict[str, Any]:
        return {
            "type": "telemetry.update",
            "timestamp": row.timestamp,
            "asset_id": row.asset_id,
            "position": {"lat": row.latitude, "lon": row.longitude, "alt": row.altitude},
            "velocity": {"heading_deg": row.heading_deg, "speed_mps": row.speed_mps},
            "track": {"confidence": row.track_confidence, "classification": "seed_track"},
            "alerts": [],
            "source": "seed",
        }

    def _playback_to_dict(self, row: PlaybackSession) -> dict[str, Any]:
        return {
            "id": row.id,
            "mode": row.mode,
            "is_running": row.is_running,
            "current_time": row.current_time,
            "start_time": row.start_time,
            "end_time": row.end_time,
            "source": row.source,
            "updated_at": row.updated_at,
        }
