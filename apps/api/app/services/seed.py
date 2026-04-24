from __future__ import annotations

from datetime import UTC, datetime, timedelta

from sqlalchemy import select
from sqlalchemy.orm import Session

from app.db.models import Alert, Asset, Event, PlaybackSession, Sensor, TelemetryPoint, Track


def seed_database(db: Session) -> None:
    existing_asset = db.scalar(select(Asset.id).limit(1))
    if existing_asset is not None:
        return

    now = datetime.now(UTC)
    assets = [
        Asset(
            id="asset-orbital-relay-1",
            name="Orbital Relay 1",
            type="satellite",
            status="active",
            latitude=25.2048,
            longitude=55.2708,
            altitude=410000.0,
            heading=142.3,
            speed=7612.4,
            metadata_json={"constellation": "Aegis Relay", "classification": "ops"},
            created_at=now,
            updated_at=now,
        ),
        Asset(
            id="asset-airborne-recon-7",
            name="Airborne Recon 7",
            type="aircraft",
            status="monitoring",
            latitude=24.9132,
            longitude=54.6111,
            altitude=15200.0,
            heading=83.5,
            speed=244.0,
            metadata_json={"role": "ISR", "callsign": "ARC-07"},
            created_at=now,
            updated_at=now,
        ),
        Asset(
            id="asset-maritime-node-3",
            name="Maritime Node 3",
            type="vessel",
            status="transit",
            latitude=24.4121,
            longitude=54.3724,
            altitude=0.0,
            heading=201.0,
            speed=17.3,
            metadata_json={"fleet": "Bluewater", "imo": "9392012"},
            created_at=now,
            updated_at=now,
        ),
    ]
    for index in range(4, 11):
        assets.append(
            Asset(
                id=f"asset-surface-node-{index}",
                name=f"Surface Node {index}",
                type="sensor_node" if index % 2 == 0 else "ground_station",
                status="active" if index % 3 else "standby",
                latitude=24.7 + (index * 0.08),
                longitude=54.1 + (index * 0.11),
                altitude=0.0,
                heading=float((index * 33) % 360),
                speed=0.0,
                metadata_json={"sector": f"S-{index}", "integration": "mission-control-seed"},
                created_at=now,
                updated_at=now,
            )
        )
    sensors = [
        Sensor(
            id="sensor-ground-radar-1",
            name="Ground Radar Alpha",
            type="radar",
            latitude=25.1548,
            longitude=55.1260,
            range_km=450.0,
            azimuth_deg=35.0,
            elevation_deg=28.0,
            is_active=True,
            metadata_json={"site": "Sector North"},
        ),
        Sensor(
            id="sensor-eoir-1",
            name="EO/IR Tower 1",
            type="eoir",
            latitude=24.9811,
            longitude=55.0033,
            range_km=120.0,
            azimuth_deg=85.0,
            elevation_deg=18.0,
            is_active=True,
            metadata_json={"site": "Harbor Gate"},
        ),
    ]
    sensors.extend(
        [
            Sensor(
                id=f"sensor-radar-{index}",
                name=f"Sector Radar {index}",
                type="radar",
                latitude=24.8 + (index * 0.07),
                longitude=54.4 + (index * 0.09),
                range_km=300.0 + index * 15,
                azimuth_deg=float((index * 22) % 360),
                elevation_deg=20.0 + index,
                is_active=True,
                metadata_json={"ring": "outer"},
            )
            for index in range(2, 6)
        ]
    )
    tracks = [
        Track(
            id="track-seed-1",
            asset_id="asset-orbital-relay-1",
            classification="orbital_vehicle",
            confidence=0.96,
            predicted_path=[
                {"lat": 25.2048, "lon": 55.2708, "alt": 410000.0},
                {"lat": 25.2248, "lon": 55.4708, "alt": 410100.0},
            ],
            last_update_at=now,
            status="confirmed",
        )
    ]
    telemetry = [
        TelemetryPoint(
            asset_id="asset-orbital-relay-1",
            timestamp=now,
            latitude=25.2048,
            longitude=55.2708,
            altitude=410000.0,
            heading_deg=142.3,
            speed_mps=7612.4,
            track_confidence=0.96,
            metadata_json={"source": "seed"},
        )
    ]
    for asset in assets[1:6]:
        telemetry.append(
            TelemetryPoint(
                asset_id=asset.id,
                timestamp=now - timedelta(minutes=1),
                latitude=asset.latitude,
                longitude=asset.longitude,
                altitude=asset.altitude,
                heading_deg=asset.heading,
                speed_mps=asset.speed,
                track_confidence=0.88,
                metadata_json={"source": "seed"},
            )
        )
    events = [
        Event(
            id="event-seed-1",
            asset_id="asset-airborne-recon-7",
            type="sensor_handoff",
            severity="info",
            title="EO/IR handoff confirmed",
            description="Track handed from radar alpha to EO/IR Tower 1 for classification refinement.",
            occurred_at=now - timedelta(minutes=5),
            source="seed",
            metadata_json={"sensor_id": "sensor-eoir-1"},
        ),
        Event(
            id="event-seed-2",
            asset_id="asset-maritime-node-3",
            type="navigation_alert",
            severity="warning",
            title="Maritime node entered monitored lane",
            description="Speed and heading crossed the monitored traffic corridor threshold.",
            occurred_at=now - timedelta(minutes=12),
            source="seed",
            metadata_json={"lane": "Delta Corridor"},
        ),
    ]
    for index in range(3, 13):
        events.append(
            Event(
                id=f"event-seed-{index}",
                asset_id=assets[index % len(assets)].id,
                type="track_event" if index % 2 else "health_event",
                severity="critical" if index % 5 == 0 else ("warning" if index % 3 == 0 else "info"),
                title=f"Mission Event {index}",
                description=f"Seeded event {index} for operator timeline rendering and playback controls.",
                occurred_at=now - timedelta(minutes=index * 3),
                source="seed",
                metadata_json={"generator": "seed_db", "ordinal": index},
            )
        )
    alerts = [
        Alert(
            id="alert-seed-1",
            event_id="event-seed-2",
            status="open",
            rule_name="corridor-crossing",
            message="Maritime Node 3 entered the monitored lane.",
            severity="warning",
            created_at=now - timedelta(minutes=12),
            metadata_json={"playbook": "surface-traffic-review"},
        )
    ]
    for index, event in enumerate(events[2:7], start=2):
        if event.severity == "info":
            continue
        alerts.append(
            Alert(
                id=f"alert-seed-{index}",
                event_id=event.id,
                status="open",
                rule_name=f"rule-{index}",
                message=event.title,
                severity=event.severity,
                created_at=event.occurred_at,
                metadata_json={"seed": True},
            )
        )
    playback = PlaybackSession(
        id="playback-primary",
        mode="live",
        start_time=now - timedelta(minutes=30),
        end_time=now,
        current_time=now,
        is_running=True,
        source="seed",
        updated_at=now,
    )

    db.add_all([*assets, *sensors, *tracks, *telemetry, *events, *alerts, playback])
    db.commit()
