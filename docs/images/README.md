# Screenshot Checklist

The README references portfolio screenshots that should be captured manually from a local, verified run. Do not add placeholder or generated images that do not show the actual repository running.

## Pending Screenshots

| File | Capture | Notes |
| --- | --- | --- |
| `aegistrack-replay-console.png` | Mission Replay Console at `/timeline` | Show tactical canvas, tracks/interceptors, assignment lines, BT panel, event feed, and timeline controls. |
| `aegistrack-focus-mode.png` | Mission Replay Console with Focus/Screenshot Mode enabled | Capture the cleaner screenshot-ready replay layout. |
| `aegistrack-tracking-visualization.png` | C++ tracking visualization, if SFML is available | Optional; only capture if the visualization target runs locally. |
| `aegistrack-profile-output.png` | Terminal or rendered output from `run_profile` | Show profile fields such as tick timing, deadline misses, and association success rate. |
| `aegistrack-test-results.png` | Terminal output from `ctest --test-dir build --output-on-failure` | Capture only after tests pass locally. |

## Manual Capture Flow

1. Run the C++ verification commands from the root README.
2. Start the frontend replay console from `apps/web`.
3. Open `http://localhost:3000/timeline`.
4. Capture the normal replay console and Focus/Screenshot Mode.
5. Save screenshots with the exact filenames above.

Screenshots are proof artifacts. Keep them current with the code and do not use them to imply unsupported runtime behavior.
