"use client";

import { useEffect } from "react";

import { sampleMissionJsonl } from "@/lib/replay/fixtures/sample-mission";
import { useReplayStore } from "@/store/replay-store";

export function ReplayFoundationStatus() {
  const dataset = useReplayStore((state) => state.dataset);
  const currentTimeS = useReplayStore((state) => state.currentTimeS);
  const loadReplayFromText = useReplayStore((state) => state.loadReplayFromText);

  useEffect(() => {
    if (!dataset) {
      loadReplayFromText(sampleMissionJsonl);
    }
  }, [dataset, loadReplayFromText]);

  return (
    <div className="rounded-2xl border border-line bg-panel/85 p-5">
      <div className="mb-4 flex items-center justify-between">
        <h2 className="text-lg font-semibold text-white">Replay Foundation</h2>
        <p className="text-xs uppercase tracking-[0.18em] text-muted">phase 1</p>
      </div>
      <div className="grid grid-cols-2 gap-3 text-sm text-muted">
        <span>Events</span>
        <span className="text-right font-mono text-white">{dataset?.events.length ?? 0}</span>
        <span>BT Decisions</span>
        <span className="text-right font-mono text-white">{dataset?.btDecisions.length ?? 0}</span>
        <span>Assignments</span>
        <span className="text-right font-mono text-white">{dataset?.assignments.length ?? 0}</span>
        <span>Faults</span>
        <span className="text-right font-mono text-white">{dataset?.faults.length ?? 0}</span>
        <span>Duration</span>
        <span className="text-right font-mono text-white">
          {dataset ? `${dataset.durationS.toFixed(2)}s` : "0.00s"}
        </span>
        <span>Playhead</span>
        <span className="text-right font-mono text-white">{currentTimeS.toFixed(2)}s</span>
        <span>Parse Errors</span>
        <span className="text-right font-mono text-white">{dataset?.errors.length ?? 0}</span>
      </div>
    </div>
  );
}
