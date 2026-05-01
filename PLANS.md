# PLANS.md

# Sensor Fusion & Multi-Target Tracking Engine (C++20) — Plan

## Objective
Build a C++20 project that simulates multiple sensors (Radar + EO/IR), ingests timestamped measurements in real time, and performs **multi-target tracking + fusion** with production-style **observability** and **deterministic replay**.

This project is designed to map directly to mission software expectations (Northrop/Anduril):
- real-time-ish processing (20–60 Hz)
- buffering/time alignment for out-of-order data
- multi-target data association
- debuggability: metrics, structured logs, replay

---

## Architecture (must match repo layout)


sensor-fusion/
CMakeLists.txt
third_party/
config/
src/
proto/ # protobuf contracts for distributed telemetry
common/ # time, IDs, RNG, config helpers, ring buffers
messages/ # structs: TruthState, Measurement, Track, Metrics snapshots
scenario/ # truth target generator + motion models
sensors/ # radar, eo/ir (optional ads-b), fault injection
transport/ # inproc message bus (later optional ZMQ/TCP)
fusion_core/ # buffering, gating, association, EKF/UKF, track manager
agents/ # interceptor and future autonomous effectors
decision/ # behavior tree decision logic (engage/acquire/idle)
observability/ # metrics registry, JSONL logging
services/ # telemetry ingest and distributed sensor nodes
viz/ # optional UI (SFML/ImGui)
tools/ # run_sim, run_replay, dump_metrics
tests/
README.md
AGENTS.md


---

## Data Contracts (src/messages/)
### TruthState
- `t` (Timestamp)
- `target_id`
- `position_xyz`, `velocity_xyz`

### Measurement
- `t_meas` (Timestamp) — measurement time in sensor clock
- `t_sent` (Timestamp) — when published (for latency simulation)
- `sensor_id`, `type`
- `z` vector
- `R` covariance
- `meta`: snr/confidence/bbox/etc.

### Track
- `t`
- `track_id`
- `x` (state), `P` (covariance)
- `quality`: age/hits/misses/score
- `last_update_by_sensor`

---

## Fusion Core Requirements (src/fusion_core/)
### State model (default)
Constant Velocity (CV), 3D:
- `x = [px, py, pz, vx, vy, vz]^T`

### Filter
- EKF (required MVP)
- UKF (optional stretch)

### Pipeline (in order)
1. **Time buffering**
2. **Predict** to fusion time
3. **Gating** (Mahalanobis distance)
4. **Association**
   - baseline: Nearest Neighbor
   - upgrade: Hungarian assignment
5. **Update** (sequential updates per measurement)
6. **Track lifecycle**
   - Tentative → Confirmed → Deleted
7. **Output tracks**
8. **Log + metrics**

---

## Observability (src/observability/)
### Metrics (at least)
- measurements/sec per sensor
- dropped/out-of-order counts
- track count (tentative/confirmed)
- association success rate
- mean gating distance
- end-to-end latency (histogram)

### Logs
- JSONL lines (one event per line)
- log events:
  - `measurement_received`
  - `track_created`
  - `track_updated`
  - `track_deleted`
- Must support deterministic replay

---

## Tools (src/tools/)
### `run_sim`
- Runs scenario + sensors + fusion in one process
- CLI flags: `--config`, `--seed`, `--duration`, `--targets`, `--hz`
- Outputs logs + optional metrics summary

### `run_replay`
- Reads JSONL log → replays measurements into fusion core
- Produces track outputs
- Must match original results (determinism test)

### `dump_metrics` (optional)
- Convert metric snapshots to CSV

---

# 6-Week Roadmap (Execution Plan)

Each week ends with **Exit Criteria** that must be met.

## Week 1 — Repo scaffold + core primitives
### Deliverables
- [ ] CMake C++20 project builds
- [ ] Directory layout created
- [ ] `Timestamp`, IDs, RNG wrapper (seeded), ring buffer
- [ ] `messages/` structs (TruthState, Measurement, Track)
- [ ] In-process `MessageBus` (pub/sub) with thread-safe queues
- [ ] `run_sim` prints truth at fixed rate (20 Hz)
- [ ] Tests: timestamp ordering, RNG determinism, queue publish/subscribe basics

### Exit criteria
- `cmake && build && ctest` all green
- deterministic run with `--seed` repeats same truth sequence

---

## Week 2 — Radar simulator + EKF single-target tracking
### Deliverables
- [ ] `scenario/` truth generator: 1 target CV motion
- [ ] `sensors/radar/` produces range/bearing measurements with noise + FOV/range
- [ ] `fusion_core/ekf/` CV EKF: predict + update
- [ ] End-to-end: truth → radar → EKF → track output/log

### Exit criteria
- Tracking converges: bounded error under noise (test asserts reasonable bound)
- Build + tests green

---

## Week 3 — Multi-target + track lifecycle + NN association
### Deliverables
- [ ] Support N targets (10–20)
- [ ] Track Manager:
  - [ ] create/confirm/delete
  - [ ] miss counters + quality score
- [ ] Mahalanobis gating
- [ ] Nearest Neighbor association baseline
- [ ] False alarms + missed detections simulation
- [ ] Metrics: track count, association success rate

### Exit criteria
- tracks persist without exploding IDs
- reasonable continuity in multi-target scenario
- tests cover track lifecycle and association baseline

---

## Week 4 — EO/IR sensor + time buffering + robustness
### Deliverables
- [x] EO/IR sensor model (bearing/elevation or image-plane proxy)
- [x] Per-sensor measurement buffers (windowed)
- [x] Fusion tick loop processes measurements ≤ fusion time
- [x] Fault injection: latency, jitter, drop, out-of-order

### Exit criteria
- system remains stable under 30% packet loss and ~150ms latency
- buffering + fusion tick tested (out-of-order handling)

---

## Week 5 — Hungarian assignment + replay mode + simple viz
### Deliverables
- [x] Hungarian algorithm association (global matching)
- [x] Structured JSONL logs for all relevant events
- [x] `run_replay` reproduces track outputs deterministically
- [x] Basic 2D visualization (optional but recommended)

### Exit criteria
- replay determinism test: identical output tracks vs original run
- improved continuity vs NN baseline (documented metric)

---

## Week 6 — Polish: metrics export, docs, demos, CI-grade tests
### Deliverables
- [x] Metrics snapshots exported to CSV
- [x] README with architecture diagram + usage commands
- [ ] Add additional tests:
  - [ ] gating math
  - [ ] EKF sanity (covariance stays PSD-ish; no NaNs)
  - [ ] Hungarian correctness on small cases
- [ ] Demo config in `config/demo.yaml`
- [x] (Optional) GitHub Actions CI to build + run tests

### Exit criteria
- One command demo works:
  - `./build/run_sim --config config/demo.yaml`
- README clearly maps features to mission software expectations
- All tests green

---

## Backlog / Stretch Goals (Optional)
- [ ] UKF implementation for non-linear sensors
- [ ] Track-to-track fusion
- [ ] Distributed transport (ZeroMQ or TCP framing)
- [ ] 3D visualization
- [ ] JPDA-lite association

---

## Notes / Decisions Log
(Keep this section updated as the build progresses.)

- Decision: test framework = (doctest or Catch2) — TBD
- Decision: EO/IR measurement representation = bearing/elevation — set
- Decision: transport (inproc only vs ZMQ) — TBD
- Decision: default association mode = Hungarian (NN retained via config toggle `--use_hungarian`) — set
- Decision: benchmarking harness = in-process repeatable A/B runner (`run_bench`) with CSV export — set
- Decision: closed-loop extension includes interceptor agent + behavior-tree engagement with kill-chain metrics — set
- Decision: mission-control web layer lives alongside the existing C++ core under `apps/api`, `apps/web`, and `packages/contracts`, using a log/profile bridge first so replay, simulation, and protobuf/gRPC flows remain intact — set
- Decision: distributed ingest path uses protobuf + gRPC client-streaming into telemetry ingest service — set
- Decision: ingest reliability uses bounded queue with drop-oldest policy + heartbeat stale detection — set
- Decision: engagement safety envelope enforces confidence/range/no-engage constraints with explicit `engage_denied` reasons — set
- Decision: unified deterministic fault injection now routes drop/delay/jitter via `FaultInjector` and global fault CLI controls — set
- Decision: incident triage path uses rolling JSONL incident capture + `run_incident_replay` with snapshot matching — set
- Decision: multi-agent coordination uses threat-scored Hungarian assignment with commit-point-based retasking and one-to-one deconfliction by default — set
- Decision: realtime instrumentation now uses per-tick profiling (`TickProfiler`), deadline miss metrics, and allocation-growth tracking (`vector_growth_events_total`) with `run_profile` JSON output — set
- Decision: optional SFML-based 2D visualizer integrated behind `--enable_viz` with headless fallback when SFML is unavailable — set
- Decision: the mission-control globe now uses offline Cesium Natural Earth imagery with an immersive HUD layout so the web view reads closer to a real-world ISR operations display while staying local-first — set
- Decision: autonomous mission logic now runs through `MissionBehaviorTree` over a shared `MissionBlackboard`; the active flow is search → track/acquire → engage with deterministic BT node JSONL logging — set
