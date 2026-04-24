from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator

from app.services.file_bridge import AegisTrackFileBridge


class TelemetryStreamService:
    def __init__(self) -> None:
        self.bridge = AegisTrackFileBridge()

    async def stream(self, interval_ms: int) -> AsyncIterator[dict]:
        index = 0
        while True:
            snapshot = self.bridge.load_snapshot()
            telemetry = snapshot.telemetry or []
            if not telemetry:
                await asyncio.sleep(interval_ms / 1000)
                continue
            yield telemetry[index % len(telemetry)]
            index += 1
            await asyncio.sleep(interval_ms / 1000)
