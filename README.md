# AegisTrack
### Real-Time Multi-Sensor Fusion, Tracking, Replay, And Mission Control

AegisTrack is a C++20 tracking and replay system with radar and EO/IR simulation, deterministic fusion, protobuf/gRPC ingest, structured JSONL observability, and benchmark tooling. This repo now also includes a mission-operations web layer that sits on top of the existing AegisTrack core.

The intent is not to replace the simulator or fusion engine. The new web layer exposes the current AegisTrack picture through a browser-oriented API and operator UI.

## Why This Project Matters

This repo now demonstrates both sides of the problem:

- backend mission software concerns: noisy sensors, latency, jitter, tracking, replay, protobuf/gRPC ingest
- operator tooling concerns: live picture, timeline, playback controls, alert surfacing, tactical geospatial view

That combination maps well to aerospace, autonomy, defense-tech, and mission-operations roles where engineers are expected to understand both runtime systems and the operator experience on top of them.

## Repository Layout

```text
sensor-fusion/
├── apps/
│   ├── api/                  # FastAPI bridge over AegisTrack logs, replay, and mission state
│   └── web/                  # Next.js mission-control frontend
├── config/                   # Existing AegisTrack scenario presets
├── deploy/                   # Existing C++ Docker assets
├── logs/                     # JSONL logs from run_sim / replay workflows
├── packages/
│   └── contracts/            # Mission-control schema documentation
├── proto/                    # Existing protobuf contracts
├── results/                  # Existing metrics / profile outputs
├── src/                      # Existing C++ tracking, fusion, replay, telemetry, viz
├── tests/                    # Existing C++ test suite
├── docker-compose.yml        # New full-stack local dev compose
├── Makefile                  # New helper targets
└── CMakeLists.txt            # Existing C++ build entrypoint
```

## Architecture

```text
[Scenario + Sensors + Fusion Core (C++)]
            |
            | run_sim / run_cluster_demo
            v
 [JSONL Logs + Metrics + Replay Outputs]
            |
            | file bridge / seed adapters
            v
      [FastAPI Mission API]
        | REST       | WebSocket
        v            v
 [Next.js Mission Control UI]
   | dashboard | globe | timeline | assets | settings
```

## Existing Core Capabilities

- radar + EO/IR simulation with latency, jitter, dropout, and false alarms
- EKF tracking with gating and configurable NN/Hungarian association
- track lifecycle management
- deterministic JSONL logging and replay
- protobuf/gRPC ingest path for distributed sensor nodes
- benchmark and realtime profiling utilities

## Mission Control Layer

### Frontend

- Next.js App Router with TypeScript
- Tailwind mission-control dark theme
- Zustand for local UI state
- TanStack Query for API data
- Cesium globe page for geospatial track rendering
- Recharts event-density timeline

Routes:

- `/dashboard`
- `/globe`
- `/timeline`
- `/assets`
- `/settings`

### Backend

- FastAPI
- WebSocket telemetry stream at `/ws/telemetry`
- REST endpoints:
  - `GET /health`
  - `GET /api/v1/assets`
  - `GET /api/v1/assets/{asset_id}`
  - `GET /api/v1/sensors`
  - `GET /api/v1/events`
  - `GET /api/v1/tracks`
  - `GET /api/v1/telemetry/latest`
  - `POST /api/v1/playback/start`
  - `POST /api/v1/playback/stop`
  - `GET /api/v1/playback/state`

### Integration Approach

The first web-facing version deliberately reuses current AegisTrack data products instead of inventing a separate fake backend:

- `run_sim` JSONL output is adapted into tracks, events, alerts, telemetry, and interceptor state
- existing metrics/profile outputs are exposed through `/health`
- SQLAlchemy models provide stable operator-facing persistence and seed data
- the bridge is isolated in `apps/api/app/services/file_bridge.py` so deeper integration can replace it later without rewriting the UI

## Local Setup

### 1. Existing C++ Build And Test Flow

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### 2. Generate AegisTrack Runtime Data

```bash
./build/run_sim --ticks 2000 --targets 8 --enable_eoir 1 --enable_eoir_updates 1 --enable_logging 1 --log_path logs/mission_control.jsonl
```

### 3. Start The Mission-Control API

```bash
cd apps/api
python -m venv .venv
source .venv/bin/activate
pip install -r requirements-dev.txt
uvicorn app.main:app --reload
```

### 4. Start The Mission-Control Web UI

```bash
cd apps/web
npm install
npm run dev
```

Open [http://localhost:3000/dashboard](http://localhost:3000/dashboard).

## Docker Setup

The existing C++ compose flow under `deploy/` remains intact.

The new root-level compose file brings up:

- `aegistrack_core`
- `postgres`
- `redis`
- `api`
- `web`

Run:

```bash
cp .env.example .env
docker compose --profile core up --build
```

## Make Targets

```bash
make up
make down
make logs
make seed
make test
make lint
make format
```

## Environment Variables

Root:

- `POSTGRES_DB`
- `POSTGRES_USER`
- `POSTGRES_PASSWORD`
- `AEGISTRACK_LOG_PATH`
- `AEGISTRACK_PROFILE_PATH`

API:

- `AEGISTRACK_API_DATABASE_URL`
- `AEGISTRACK_API_REDIS_URL`
- `AEGISTRACK_API_LOG_PATH`
- `AEGISTRACK_API_PROFILE_PATH`
- `AEGISTRACK_API_ALLOWED_ORIGINS`
- `AEGISTRACK_API_ENABLE_FILE_BRIDGE`

Web:

- `NEXT_PUBLIC_API_BASE_URL`
- `NEXT_PUBLIC_WS_BASE_URL`
- `NEXT_PUBLIC_CESIUM_BASE_URL`

## Screenshots

Placeholders for future captures:

- `docs/screenshots/dashboard.png`
- `docs/screenshots/globe.png`
- `docs/screenshots/timeline.png`
- `docs/screenshots/assets.png`

## Testing

### C++

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### API

```bash
cd apps/api
pytest
```

### Frontend

Initial test structure is documented in:

- [apps/web/src/tests/README.md](/Users/eduardoescobar/Projects/sensor-fusion/apps/web/src/tests/README.md:1)

## Future Roadmap

- replace the JSONL bridge with a direct stream from ingest/fusion runtime state
- add live benchmark and health panels from the metrics registry
- connect replay controls to `run_replay` and incident-capture workflows
- add operator acknowledgment flow for alerts and playbooks
- add richer Cesium overlays for sensor frustums and intercept geometry
