# AGENTS.md

This repository is a C++20 project for a **Real-Time Multi-Sensor Fusion & Multi-Target Tracking Engine**.
Codex should follow these rules when making changes.

## Non-negotiables
1. **C++20 + CMake only** (no Bazel, no Meson).
2. Keep the architecture modular using the directory layout in `PLANS.md`.
3. **Every task must include tests** (unit tests preferred) and must end with a clean build + test run.
4. Prefer small, reviewable diffs. Avoid “big bang” refactors.
5. Do not introduce new third-party dependencies unless explicitly requested.

## Verification (must run before declaring a task done)
Run from repo root:
- `cmake -S . -B build`
- `cmake --build build -j`
- `ctest --test-dir build --output-on-failure`

If tests fail, fix and rerun until green.

## Dependencies (allowed)
- **Eigen** (linear algebra) — header-only
- **spdlog** (logging) — header-only ok
- **nlohmann/json** (config + JSONL logs) — header-only
- (Optional later) **SFML** or **Dear ImGui** for visualization
- (Optional later) **ZeroMQ** or **Boost.Asio** for transport
If a dependency is added, it must be vendored in `third_party/` or via CMake FetchContent, and documented in README.

## Style & quality
- Use `std::chrono` and a single `Timestamp` abstraction in `src/common/time.h`.
- Avoid global state. Prefer dependency injection for clocks, RNG, and configuration.
- Use RAII, `std::unique_ptr`, `std::shared_ptr` only where ownership is shared by design.
- Thread safety:
  - Transport queues must be thread-safe.
  - Shared structures must be protected (mutex) or designed as immutable snapshots.
- Logging:
  - Use structured JSONL logs for measurements + track updates.
  - Logs must be deterministic given a fixed seed (no nondeterministic ordering).
- Determinism:
  - All randomness must come from a single seeded RNG passed down from main/config.
  - No use of `rand()`.

## Tests (required)
Use a C++ unit test framework (pick one):
- Option A: lightweight single-header (preferred) like `doctest`
- Option B: `Catch2` via FetchContent

Minimum required tests as modules are added:
- `common`: Timestamp ordering, RNG determinism, ring buffer behavior
- `transport`: publish/subscribe order and multi-subscriber delivery
- `fusion_core`: EKF predict/update sanity, gating correctness, association correctness
- `track_manager`: create/confirm/delete, miss counters, quality score progression
- `replay`: deterministic replay produces identical track outputs

## File/Module boundaries (do not blur)
- `src/messages/` contains **data contracts only** (no business logic).
- `src/sensors/` contains sensor simulation only.
- `src/fusion_core/` contains tracking, filtering, association, and lifecycle.
- `src/observability/` contains metrics + logging utilities.
- `src/tools/` contains executables (run_sim, run_replay, etc.).

## Default build targets
- `run_sim` (primary executable)
- `run_replay` (added later)
- `tests` / `ctest` always available

## Commit hygiene
When finishing a task:
- Ensure the repo builds and tests pass.
- Update `PLANS.md` checkboxes and notes for what was completed.
- Keep README accurate if commands change.
