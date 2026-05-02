# Results Artifacts

This directory is for locally generated benchmark and profile outputs that support resume or interview claims. Do not commit fabricated numbers. Regenerate artifacts from the current source tree before citing any metric.

## Expected Artifacts

| Artifact | Generator | Purpose |
| --- | --- | --- |
| `bench.csv` | `./build/run_bench --out_csv results/bench.csv` | Compares radar-only, radar+EOIR, Hungarian, and nearest-neighbor benchmark scenarios. |
| `profile.json` | `./build/run_profile --out results/profile.json` | Captures tick timing, deadline misses, association success rate, and queue-drop profile data. |
| `load_test.txt` | `./build/run_load_test ...` | Captures telemetry ingest throughput, published/consumed counts, and queue drops. |
| `simulation.log` or `mission_control.jsonl` | `./build/run_sim --enable_logging 1 --log_path logs/mission_control.jsonl` | Captures deterministic simulator JSONL logs for replay/debugging. |

Generated CSV, JSON, JSONL, and text outputs are intentionally ignored by git unless a reviewer explicitly asks to preserve a specific artifact.

## Regeneration Commands

Run from the repository root after configuring and building the C++ targets:

```bash
./build/run_bench --runs 5 --seed 1 --out_csv results/bench.csv
./build/run_profile --config config/demo_radar_eoir.json --seed 1 --ticks 500 --out results/profile.json
./build/run_load_test --virtual_sensors 20 --hz 200 --duration_s 10 --queue_max 10000 > results/load_test.txt
./build/run_sim --enable_logging 1 --log_path logs/mission_control.jsonl --confirmed_delete_misses 10
```

## Resume-Safe Metrics

Only cite these after regenerating them locally and preserving the command/output in your notes:

- `deadline_misses`
- `mean_tick_ms` and `max_tick_ms`
- `ticks_per_sec`
- `assoc_success_rate`
- `queue_drops`
- `successful_intercepts_total`
- `possible_id_switch_total`
- `track_fragmentation_warnings_total`
- `tracks_confirmed_avg_age_s`

Metrics are simulation-specific diagnostics, not general production performance guarantees.
