# Interview Pitch

## 60–90 second pitch
I built `sensor-fusion`, a C++20 real-time-ish multi-sensor fusion and multi-target tracking engine designed to mirror mission software constraints. It simulates radar and EO/IR streams with latency, jitter, drops, and out-of-order delivery, then tracks targets through a constant-velocity EKF pipeline with gating, global association, and lifecycle management.

The hard part is not just filtering; it is maintaining track quality when data is delayed, noisy, and ambiguous. I addressed that with Mahalanobis gating, configurable nearest-neighbor or Hungarian assignment, and deterministic buffering/replay semantics so behavior is reproducible and debuggable.

I also treated observability as a first-class requirement: structured JSONL event logs, replay tooling, benchmark harnesses, and CSV metrics output make regressions and performance changes measurable. In practice, EO/IR refinement lowers average position error and Hungarian association improves assignment success under clutter compared to greedy matching.

## Deep-dive talking points
- Mahalanobis gating: Normalizes innovation by covariance so thresholds scale with uncertainty, not raw distance.
- Hungarian vs NN: NN is simpler/cheaper but order-sensitive; Hungarian is globally optimal for one-to-one assignment.
- EKF radar model: Nonlinear range/bearing measurement with hand-derived Jacobian and wrapped bearing residual.
- EKF EO/IR model: Bearing/elevation Jacobian improves angular refinement without creating EOIR-only tracks.
- Replay determinism: JSONL measurements + ordered fusion-time processing reproduces states for debugging and regression tests.
- Failure simulation: Latency, jitter, drop, false alarms, and out-of-order arrival are injected in sensor simulators.
- Lifecycle controls: Tentative/confirmed/deleted track states are driven by hits/misses and configurable thresholds.
- Evolution path: UKF/JPDA for denser ambiguity, distributed transport, and stronger observability/CI gates.

## Likely interview questions + answers
- Q: How do you debug track swaps?
  A: I inspect association costs/gates per tick, compare NN vs Hungarian behavior, and replay the exact measurement log deterministically.

- Q: What happens if measurements arrive late?
  A: Per-sensor buffers hold data by sent time; fusion consumes only ready measurements at fusion time, preserving stable ordering.

- Q: Why EKF instead of a particle filter?
  A: EKF is efficient and interpretable for this state/measurement regime; it fits real-time constraints and is easy to validate in CI.

- Q: How would you scale this to distributed nodes?
  A: Keep message contracts stable, add transport abstraction with timestamp discipline, and preserve replayability via centralized event logging.

- Q: How do you compare configurations objectively?
  A: Use in-process repeatable A/B benchmark runs with fixed seeds and CSV metrics (`pos_error`, association success, confirmed tracks).
