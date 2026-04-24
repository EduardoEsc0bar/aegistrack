# TRIAGE.md

## Triage Playbook

Use this checklist when fusion quality or cluster reliability regresses.

## First Metrics To Check
- `ingest_queue_depth`: rising trend means ingest is falling behind.
- `ingest_queue_drops_total`: non-zero growth means data loss at gateway.
- `sensor_stale_total` and `sensor_last_seen_age_s_<sensor_id>`: identifies silent nodes.
- `assoc_success` / `assoc_attempts`: association quality under clutter.
- `pos_error` observation (`mean/min/max`): end result tracking quality.

## Common Failure Modes
- Track swaps under clutter:
  - Symptoms: lower association success and jumpy confirmed tracks.
  - Typical causes: weak gating threshold or delayed bursts.
- Late burst data causing association instability:
  - Symptoms: out-of-order ingest spikes and temporary track churn.
  - Typical causes: burst + reorder + high queue pressure.
- Sensor silent/stale:
  - Symptoms: stale counter increments, sensor last-seen age climbs.
  - Typical causes: node crash, stream break, network partition.

## Debug Workflow
1. Confirm reliability state:
   - Run cluster and inspect ingest metrics (`ingest_queue_depth`, drops, stale).
2. Reproduce deterministically:
   - Use fixed seeds in `sensor_node` and `run_load_test`.
3. Inspect event history:
   - Use JSONL logs from simulation and replay with `run_replay`.
4. Compare algorithm configs:
   - Use `run_bench` for NN vs Hungarian and radar-only vs radar+EOIR.
5. Validate throughput margin:
   - Use `run_load_test` with stress knobs (`--virtual_sensors`, `--hz`, burst/drop/reorder).

## Incident Examples

### Incident 1: Sensor node stops heartbeating
- Signal:
  - `sensor_last_seen_age_s_<id>` exceeds 2s.
  - `sensor_stale_total` increments.
- Actions:
  1. Confirm `sensor_node` process health and gRPC connectivity.
  2. Restart or isolate the stale node.
  3. Verify heartbeats resume (age returns near 0).
  4. Check if fusion quality recovers (association + confirmed tracks).

### Incident 2: Ingest queue drops spike under 20 sensors @ 200 Hz
- Signal:
  - `ingest_queue_depth` near max continuously.
  - `ingest_queue_drops_total` increasing quickly.
- Actions:
  1. Reproduce with:
     - `./build/run_load_test --virtual_sensors 20 --hz 200 --duration_s 10 --seed 1`
  2. Evaluate whether `queue_max` should be increased or sensor rates reduced.
  3. Check burst/reorder/drop knobs for excessive stress settings.
  4. Verify throughput after tuning by comparing `rate_dps` and drops.
