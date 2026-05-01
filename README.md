# AegisTrack

Real-time C++20 sensor-fusion and mission-autonomy simulation with deterministic replay, behavior-tree decision logic, multi-target tracking, interceptor assignment, and a TypeScript Mission Replay Console.

## Hero Screenshots

Screenshots should be added manually after running the web console locally.

![AegisTrack Mission Replay Console](docs/images/aegistrack-replay-console.png)

**Mission Replay Console:** TypeScript/Next.js frontend visualizing deterministic C++ JSONL replay data with tracks, interceptors, assignment lines, injected faults, intercepts, and behavior-tree decisions.

![AegisTrack Focus Mode](docs/images/aegistrack-focus-mode.png)

**Focus Mode:** Screenshot-ready replay view centered on the tactical canvas and behavior-tree decision trace.

## Overview

AegisTrack is a simulation project for multi-sensor tracking and mission autonomy. The C++ core simulates radar and EO/IR sensing, injects jitter/drop faults, tracks multiple targets with an EKF-based pipeline, and records deterministic JSONL logs for replay and diagnostics.

On top of the simulation core, AegisTrack includes a behavior-tree autonomy layer with stateful mission memory, multi-engagement interceptor assignment, duplicate assignment suppression, and intercept-aware engagement scoring. A TypeScript Mission Replay Console visualizes replay logs as a scrub-able engineering dashboard.

The project is intended to demonstrate real-time systems thinking, tracking diagnostics, replayability, and practical mission-autonomy tradeoffs. It is a simulation and diagnostics environment, not an operational defense system.

## Key Features

- C++20 real-time simulation loop using CMake.
- Radar and EO/IR sensor simulation.
- Fault injection for measurement jitter and packet/drop behavior.
- EKF-based multi-target tracking.
- Mahalanobis gating and association diagnostics.
- Track lifecycle stability, confirmed-track coasting, and deletion tuning.
- Fragmentation and possible ID-switch diagnostics.
- Behavior-tree autonomy with mission blackboard state.
- Multi-engagement interceptor assignment.
- Duplicate assignment suppression and reconciliation hardening.
- Intercept-aware engagement scoring.
- Deterministic JSONL logging and replay-oriented event contracts.
- Metrics output for tracking, autonomy, intercept, and timing diagnostics.
- TypeScript/Next.js Mission Replay Console for local replay visualization.

## Architecture

```text
Radar / EOIR Simulation
        |
        v
Fault Injection: jitter + drop
        |
        v
Measurement Buffer
        |
        v
EKF Tracker + Mahalanobis Gating + Association
        |
        v
Track Lifecycle Manager
        |
        v
Mission Blackboard
        |
        v
Behavior Tree Autonomy
        |
        v
Interceptor Assignment + Engagement Scoring
        |
        v
Metrics + Deterministic JSONL Logs
        |
        v
Mission Replay Console
```

## System Components

### C++ Simulation Core

The core simulation models a multi-sensor tracking pipeline in C++20. It includes radar and EO/IR measurements, injected sensing degradation, measurement buffering, EKF prediction/update behavior, Mahalanobis gating, association diagnostics, and track lifecycle management.

The tracking layer emits structured events for track updates, stability transitions, coasting behavior, possible fragmentation, and deletion outcomes. Metrics are used to compare tuning changes rather than to claim universal performance.

### Mission Autonomy Layer

The autonomy layer models mission decision logic using behavior-tree execution and a mission blackboard. It tracks active engagements, suppresses duplicate assignments, reconciles completed intercepts, and scores candidate interceptor-track pairings with intercept-aware cost terms.

This layer is designed to make assignment behavior inspectable: decisions are logged with selected tracks, selected interceptors, engagement scores, estimated intercept times, node traces, and assignment reasons.

### Mission Replay Console

The Mission Replay Console is a Next.js/React/TypeScript frontend for replaying deterministic C++ JSONL logs. It visualizes tracks, interceptors, assignment lines, intercept events, injected faults, behavior-tree decisions, a replay timeline, and a compact event stream.

The console runs locally and does not require a live backend for the included sample replay.

## Metrics and Results

The numbers below are simulation-specific comparisons from recent tuning passes. They are useful for showing engineering tradeoffs across track stability, autonomy behavior, and engagement scoring; they are not general performance guarantees.

### Tracking Stability

| Metric | Before | After |
| --- | ---: | ---: |
| Possible ID switches | 59 | 2 |
| Fragmentation warnings | 59 | 2 |
| Confirmed average track age | 0.25s | 3.19s |
| Confirmed tracks created | 64 | 19 |
| Tracks deleted | 75 | 12 |

### Engagement

| Metric | Before | After |
| --- | ---: | ---: |
| Successful intercepts | 5 | 8 |
| Average time to intercept | 1360ms | 1031ms |
| Deadline misses | 0 | 0 |

Later reconciliation and hardening work also aligned `bt_active_engagements` with `interceptors_engaged` while preserving zero deadline misses in the measured scenario.

## Example Runtime Output

Example metrics block from a simulation run:

```text
deadline_misses=0
successful_intercepts_total=8
possible_id_switch_total=2
track_fragmentation_warnings_total=2
tracks_confirmed_avg_age_s=3.19
```

## Running Locally

### C++ Simulation

```bash
cmake -S . -B build
cmake --build build -j1
./build/run_sim --confirmed_delete_misses 10
```

`-j1` is recommended on low-memory machines. Use a higher parallelism value only when the local machine has enough memory for the build.

### Mission Replay Console

```bash
cd apps/web
npm install
npm run dev
```

Then open:

```text
http://localhost:3000/timeline
```

The Timeline page loads the included sample replay fixture and renders the Mission Replay Console without requiring a live backend.

## Replay Data

The replay parser supports these JSONL event types:

- `measurement`
- `track_event`
- `track_stability_event`
- `interceptor_state`
- `assignment_event`
- `intercept_event`
- `bt_decision`
- `fault_event`
- `demo_metadata`

The included demo replay currently contains:

| Replay Content | Count |
| --- | ---: |
| Duration | 8.0s |
| Tracks | 5 |
| Interceptors | 3 |
| Injected faults | 5 |
| Intercepts | 3 |
| JSONL events | 82 |

The fixture is a degraded-sensing multi-engagement replay intended to make assignment lines, behavior-tree decisions, coasting, faults, and intercept outcomes visible immediately in the frontend.

## Design Decisions

### Why Behavior Trees?

Behavior trees make autonomy decisions inspectable and modular. For this project, they provide a practical way to represent search, track, engage, maintain, retask, and reconciliation behavior while logging the decision path for replay.

### Why Deterministic JSONL Replay?

Replay logs make complex tracking and autonomy behavior debuggable. JSONL events can be inspected, diffed, loaded by the frontend, and used to reproduce a specific mission timeline without depending on a live simulator process.

### Why Track Lifecycle Tuning?

Raw detection-to-track behavior can produce fragmentation, short-lived confirmed tracks, and possible ID switches under degraded sensing. Lifecycle tuning, coasting, and stability diagnostics make those tradeoffs measurable.

### Why Intercept-Aware Scoring?

Naive assignment can engage the wrong target or leave interceptors poorly allocated. Intercept-aware scoring incorporates engagement feasibility and estimated time-to-intercept so assignment decisions better reflect mission outcomes in the simulation.

## Tech Stack

### Core

- C++20
- CMake
- EKF tracking
- Mahalanobis gating
- Deterministic JSONL logging
- Metrics and replay diagnostics

### Frontend

- Next.js
- React
- TypeScript
- Zustand
- Tailwind CSS
- Vitest

## Future Work

- Live C++ to frontend bridge.
- Richer radar and EO/IR sensor models.
- More scenario configuration presets.
- CI coverage for core simulation and web replay parser behavior.
- More realistic interceptor dynamics.
- Advanced data association experiments.
- Exportable replay reports for incidents and demonstrations.
- Additional screenshot and GIF assets for README and portfolio use.

## Resume Summary

AegisTrack is a C++20 real-time simulation and replay project covering sensor fusion, target tracking, mission autonomy, interceptor assignment, deterministic logging, metrics-driven tuning, and a TypeScript replay dashboard. It demonstrates practical engineering work across simulation, autonomy decision logic, diagnostics, and frontend visualization.
