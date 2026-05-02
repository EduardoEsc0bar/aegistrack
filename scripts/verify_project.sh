#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build}"

section() {
  printf '\n== %s ==\n' "$1"
}

run_core() {
  section "$1"
  shift
  "$@"
}

skip_optional() {
  section "$1"
  printf 'skipped: %s\n' "$2"
}

cd "$ROOT_DIR"

run_core "Configure CMake" cmake -S . -B "$BUILD_DIR" -DAEGISTRACK_VIZ=OFF
run_core "Build C++ targets" cmake --build "$BUILD_DIR" -j1
run_core "Run C++ tests" ctest --test-dir "$BUILD_DIR" --output-on-failure

if [[ -x "$BUILD_DIR/run_sim" ]]; then
  run_core "Run deterministic simulation smoke" "$BUILD_DIR/run_sim" \
    --ticks 200 \
    --enable_logging 1 \
    --log_path logs/verify_mission_control.jsonl \
    --confirmed_delete_misses 10 \
    --enable_viz 0
else
  skip_optional "Run deterministic simulation smoke" "$BUILD_DIR/run_sim was not built"
fi

if [[ -x "$BUILD_DIR/run_replay" && -f logs/verify_mission_control.jsonl ]]; then
  run_core "Run replay smoke" "$BUILD_DIR/run_replay" --input logs/verify_mission_control.jsonl
else
  skip_optional "Run replay smoke" "run_replay binary or logs/verify_mission_control.jsonl is missing"
fi

if [[ -x "$BUILD_DIR/run_bench" ]]; then
  run_core "Run benchmark smoke" "$BUILD_DIR/run_bench" --runs 1 --seed 1 --out_csv results/bench.csv
else
  skip_optional "Run benchmark smoke" "$BUILD_DIR/run_bench was not built"
fi

if [[ -x "$BUILD_DIR/run_profile" ]]; then
  run_core "Run profile smoke" "$BUILD_DIR/run_profile" \
    --config config/demo_radar_eoir.json \
    --seed 1 \
    --ticks 200 \
    --out results/profile.json
else
  skip_optional "Run profile smoke" "$BUILD_DIR/run_profile was not built"
fi

if [[ -x "$BUILD_DIR/run_load_test" ]]; then
  run_core "Run telemetry load-test smoke" "$BUILD_DIR/run_load_test" \
    --virtual_sensors 2 \
    --hz 20 \
    --duration_s 1 \
    --queue_max 1000
else
  skip_optional "Run telemetry load-test smoke" "$BUILD_DIR/run_load_test was not built"
fi

if [[ -d apps/web && -f apps/web/package.json ]]; then
  section "Frontend replay console checks"
  if [[ -d apps/web/node_modules ]]; then
    (cd apps/web && npm test)
  else
    printf 'skipped: apps/web/node_modules is missing; run npm install or npm ci before frontend checks\n'
  fi
else
  skip_optional "Frontend replay console checks" "apps/web/package.json is missing"
fi

section "Verification complete"
printf 'Core configure/build/test and available smoke checks completed.\n'
