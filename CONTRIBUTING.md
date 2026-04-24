# Contributing

## Build and test
Run from repo root:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Code style
- Use C++20 with CMake only.
- Keep modules aligned to `src/common`, `src/messages`, `src/sensors`, `src/fusion_core`, `src/observability`, and `src/tools`.
- Prefer small, reviewable diffs.

## Determinism requirements
- All randomness must flow through seeded RNG wrappers.
- Avoid nondeterministic ordering in logs or processing.
- Replay results should be reproducible with the same inputs.

## Tests
- Add or update tests for each behavioral change.
- Favor unit tests for `common`, `transport`, `fusion_core`, and `track_manager` behavior.
