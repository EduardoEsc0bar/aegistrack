from __future__ import annotations

import json
import math
from dataclasses import dataclass
from datetime import UTC, datetime, timedelta
from pathlib import Path
from typing import Any

from app.core.config import get_settings


@dataclass(slots=True)
class RuntimeSnapshot:
    assets: list[dict[str, Any]]
    sensors: list[dict[str, Any]]
    tracks: list[dict[str, Any]]
    events: list[dict[str, Any]]
    alerts: list[dict[str, Any]]
    telemetry: list[dict[str, Any]]
    bridge_status: dict[str, Any]


class AegisTrackFileBridge:
    def __init__(self) -> None:
        self.settings = get_settings()

    def load_snapshot(self) -> RuntimeSnapshot:
        log_path = Path(self.settings.log_path)
        if not self.settings.enable_file_bridge or not log_path.exists():
            return RuntimeSnapshot([], [], [], [], [], [], {"enabled": False, "log_path": str(log_path)})

        lines = [
            json.loads(line)
            for line in log_path.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        return self._build_snapshot(lines, log_path)

    def _build_snapshot(self, lines: list[dict[str, Any]], log_path: Path) -> RuntimeSnapshot:
        now = datetime.now(UTC)
        sensors: dict[str, dict[str, Any]] = {}
        tracks: dict[str, dict[str, Any]] = {}
        assets: dict[str, dict[str, Any]] = {}
        telemetry: list[dict[str, Any]] = []
        events: list[dict[str, Any]] = []
        alerts: list[dict[str, Any]] = []

        for entry in lines[-1500:]:
            entry_type = entry.get("type")
            if entry_type == "measurement":
                sensor = self._sensor_from_measurement(entry)
                sensors[sensor["id"]] = sensor
            elif entry_type == "track_event":
                track = self._track_from_event(entry)
                tracks[track["id"]] = track
                asset = self._asset_from_track(track, now)
                assets[asset["id"]] = asset
                telemetry.append(self._telemetry_from_track(track))
                events.append(self._event_from_track(entry, track))
            elif entry_type in {"decision_event", "intercept_event", "assignment_event"}:
                event = self._event_from_runtime(entry)
                events.append(event)
                if event["severity"] in {"warning", "critical"}:
                    alerts.append(self._alert_from_event(event))
            elif entry_type == "interceptor_state":
                asset = self._asset_from_interceptor(entry, now)
                assets[asset["id"]] = asset
                telemetry.append(self._telemetry_from_interceptor(entry))

        return RuntimeSnapshot(
            assets=sorted(assets.values(), key=lambda item: item["id"]),
            sensors=sorted(sensors.values(), key=lambda item: item["id"]),
            tracks=sorted(tracks.values(), key=lambda item: item["id"]),
            events=sorted(events, key=lambda item: item["occurred_at"], reverse=True)[:200],
            alerts=sorted(alerts, key=lambda item: item["created_at"], reverse=True)[:50],
            telemetry=sorted(telemetry, key=lambda item: item["timestamp"], reverse=True)[:50],
            bridge_status={
                "enabled": True,
                "log_path": str(log_path),
                "records_seen": len(lines),
                "last_modified": datetime.fromtimestamp(log_path.stat().st_mtime, tz=UTC),
            },
        )

    def _sensor_from_measurement(self, entry: dict[str, Any]) -> dict[str, Any]:
        sensor_id = f"sensor-{int(entry['sensor_id'])}"
        sensor_type = str(entry.get("sensor_type", "Radar")).lower()
        lat, lon, _ = self._xy_to_geodetic(entry["sensor_id"] * 20.0, entry["sensor_id"] * 15.0, 0.0)
        return {
            "id": sensor_id,
            "name": f"{entry.get('sensor_type', 'Sensor')} Node {entry['sensor_id']}",
            "type": sensor_type,
            "latitude": lat,
            "longitude": lon,
            "range_km": 380.0 if sensor_type == "radar" else 120.0,
            "azimuth_deg": 45.0,
            "elevation_deg": 30.0,
            "is_active": True,
            "metadata": {
                "measurement_type": entry.get("measurement_type"),
                "confidence": entry.get("confidence"),
            },
        }

    def _track_from_event(self, entry: dict[str, Any]) -> dict[str, Any]:
        x = entry.get("x", [0.0] * 6)
        lat, lon, alt = self._xy_to_geodetic(x[0], x[1], x[2])
        vx = x[3] if len(x) > 3 else 0.0
        vy = x[4] if len(x) > 4 else 0.0
        vz = x[5] if len(x) > 5 else 0.0
        path = []
        for step in range(1, 7):
            px = x[0] + vx * step * 3.0
            py = x[1] + vy * step * 3.0
            pz = x[2] + vz * step * 3.0
            step_lat, step_lon, step_alt = self._xy_to_geodetic(px, py, pz)
            path.append({"lat": step_lat, "lon": step_lon, "alt": step_alt})
        return {
            "id": f"track-{entry['track_id']}",
            "asset_id": f"track-{entry['track_id']}",
            "classification": self._classify_track(alt),
            "confidence": float(entry.get("confidence", 0.0)),
            "status": str(entry.get("status", "Tentative")).lower(),
            "predicted_path": path,
            "last_update_at": self._sim_time_to_datetime(float(entry.get("t_s", 0.0))),
            "latitude": lat,
            "longitude": lon,
            "altitude": alt,
            "heading": math.degrees(math.atan2(vy, vx)) if abs(vx) + abs(vy) > 0 else 90.0,
            "speed": math.sqrt(vx * vx + vy * vy + vz * vz),
        }

    def _asset_from_track(self, track: dict[str, Any], now: datetime) -> dict[str, Any]:
        return {
            "id": track["asset_id"],
            "name": f"Aegis Track {track['id'].split('-')[-1]}",
            "type": track["classification"],
            "status": "active" if track["status"] != "deleted" else "inactive",
            "latitude": track["latitude"],
            "longitude": track["longitude"],
            "altitude": track["altitude"],
            "heading": track["heading"],
            "speed": track["speed"],
            "metadata": {"source": "jsonl_bridge", "track_id": track["id"]},
            "created_at": now,
            "updated_at": track["last_update_at"],
        }

    def _asset_from_interceptor(self, entry: dict[str, Any], now: datetime) -> dict[str, Any]:
        position = entry.get("position", [0.0, 0.0, 0.0])
        velocity = entry.get("velocity", [0.0, 0.0, 0.0])
        lat, lon, alt = self._xy_to_geodetic(position[0], position[1], position[2])
        heading = math.degrees(math.atan2(velocity[1], velocity[0])) if velocity[:2] else 0.0
        speed = math.sqrt(sum(component * component for component in velocity))
        return {
            "id": f"interceptor-{entry['interceptor_id']}",
            "name": f"Interceptor {entry['interceptor_id']}",
            "type": "interceptor",
            "status": "engaged" if int(entry.get("engaged", 0)) else "idle",
            "latitude": lat,
            "longitude": lon,
            "altitude": alt,
            "heading": heading,
            "speed": speed,
            "metadata": {"track_id": entry.get("track_id"), "source": "jsonl_bridge"},
            "created_at": now,
            "updated_at": self._sim_time_to_datetime(float(entry.get("t_s", 0.0))),
        }

    def _telemetry_from_track(self, track: dict[str, Any]) -> dict[str, Any]:
        return {
            "type": "telemetry.update",
            "timestamp": track["last_update_at"],
            "asset_id": track["asset_id"],
            "position": {"lat": track["latitude"], "lon": track["longitude"], "alt": track["altitude"]},
            "velocity": {"heading_deg": track["heading"], "speed_mps": track["speed"]},
            "track": {"confidence": track["confidence"], "classification": track["classification"]},
            "alerts": [],
            "source": "aegistrack-jsonl",
        }

    def _telemetry_from_interceptor(self, entry: dict[str, Any]) -> dict[str, Any]:
        position = entry.get("position", [0.0, 0.0, 0.0])
        velocity = entry.get("velocity", [0.0, 0.0, 0.0])
        lat, lon, alt = self._xy_to_geodetic(position[0], position[1], position[2])
        speed = math.sqrt(sum(component * component for component in velocity))
        heading = math.degrees(math.atan2(velocity[1], velocity[0])) if velocity[:2] else 0.0
        return {
            "type": "telemetry.update",
            "timestamp": self._sim_time_to_datetime(float(entry.get("t_s", 0.0))),
            "asset_id": f"interceptor-{entry['interceptor_id']}",
            "position": {"lat": lat, "lon": lon, "alt": alt},
            "velocity": {"heading_deg": heading, "speed_mps": speed},
            "track": {"confidence": 1.0, "classification": "interceptor"},
            "alerts": [],
            "source": "aegistrack-jsonl",
        }

    def _event_from_track(self, entry: dict[str, Any], track: dict[str, Any]) -> dict[str, Any]:
        event_type = entry.get("event", "track_updated")
        severity = "info"
        if event_type == "track_deleted":
            severity = "warning"
        return {
            "id": f"event-{entry.get('trace_id', track['id'])}",
            "asset_id": track["asset_id"],
            "type": event_type,
            "severity": severity,
            "title": f"{event_type.replace('_', ' ').title()}",
            "description": f"{track['id']} {event_type.replace('_', ' ')} with confidence {track['confidence']:.2f}.",
            "occurred_at": track["last_update_at"],
            "source": "track_event",
            "metadata": {"status": track["status"]},
        }

    def _event_from_runtime(self, entry: dict[str, Any]) -> dict[str, Any]:
        sim_time = self._sim_time_to_datetime(float(entry.get("t_s", 0.0)))
        event_type = str(entry.get("type"))
        severity = "info"
        title = event_type.replace("_", " ").title()
        description = "AegisTrack runtime event."
        if event_type == "decision_event" and entry.get("decision_type") == "engage_denied":
            severity = "warning"
            description = f"Engagement denied: {entry.get('reason', 'policy gate')}"
        elif event_type == "intercept_event":
            severity = "critical" if entry.get("outcome") != "intercept_success" else "warning"
            description = f"{entry.get('outcome', 'intercept')} because {entry.get('reason', 'unspecified')}"
        elif event_type == "assignment_event":
            description = f"Interceptor {entry.get('interceptor_id')} {entry.get('decision_type')}"
        return {
            "id": f"event-{entry.get('trace_id', entry.get('t_s', 0.0))}",
            "asset_id": f"track-{entry['track_id']}" if entry.get("track_id") else None,
            "type": event_type,
            "severity": severity,
            "title": title,
            "description": description,
            "occurred_at": sim_time,
            "source": event_type,
            "metadata": entry,
        }

    def _alert_from_event(self, event: dict[str, Any]) -> dict[str, Any]:
        return {
            "id": f"alert-{event['id']}",
            "event_id": event["id"],
            "status": "open",
            "rule_name": event["type"],
            "message": event["description"],
            "severity": event["severity"],
            "created_at": event["occurred_at"],
            "metadata": {"source": event["source"]},
        }

    def _sim_time_to_datetime(self, seconds: float) -> datetime:
        return datetime(2026, 4, 16, 12, 0, tzinfo=UTC) + timedelta(seconds=seconds)

    def _xy_to_geodetic(self, x: float, y: float, z: float) -> tuple[float, float, float]:
        # The simulator operates in local meters. Use a display scale so operator views
        # show separation between tracks instead of collapsing into a single pixel.
        display_scale_m_per_degree = 18_000.0
        lat = self.settings.anchor_latitude + (y / display_scale_m_per_degree)
        lon = self.settings.anchor_longitude + (
            x / (display_scale_m_per_degree * math.cos(math.radians(lat)))
        )
        alt = max(0.0, z)
        return lat, lon, alt

    def _classify_track(self, altitude: float) -> str:
        if altitude > 100_000.0:
            return "orbital_vehicle"
        if altitude > 15_000.0:
            return "high_altitude_aircraft"
        return "airborne_track"
