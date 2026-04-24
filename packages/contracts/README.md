# AegisTrack Mission Control Contracts

This package documents the operator-facing payloads exposed by the mission-control layer added to AegisTrack.

Sources of truth:

- `apps/api/app/schemas/`: Python response and WebSocket schemas
- `apps/web/src/lib/types.ts`: TypeScript client models
- `packages/contracts/mission-control.schema.json`: JSON schema snapshot for external tooling

The contracts intentionally reuse AegisTrack concepts:

- tracks
- sensors
- telemetry measurements
- replay / playback state
- alerts and decision events
- interceptor and assignment activity
