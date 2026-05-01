"use client";

import { useEffect, useMemo, useState } from "react";

import { ReplayBtPanel } from "@/components/replay/replay-bt-panel";
import { ReplayEventFeed } from "@/components/replay/replay-event-feed";
import { ReplayMetricsStrip } from "@/components/replay/replay-metrics-strip";
import { ReplayTacticalCanvas } from "@/components/replay/replay-tactical-canvas";
import { ReplayTimelineControls } from "@/components/replay/replay-timeline-controls";
import { sampleMissionJsonl } from "@/lib/replay/fixtures/sample-mission";
import { getReplayFrameAt } from "@/lib/replay/selectors";
import type { ReplayDataset } from "@/lib/replay/types";
import { useReplayStore } from "@/store/replay-store";

export function ReplayMissionView() {
  const dataset = useReplayStore((state) => state.dataset);
  const currentTimeS = useReplayStore((state) => state.currentTimeS);
  const isPlaying = useReplayStore((state) => state.isPlaying);
  const playbackSpeed = useReplayStore((state) => state.playbackSpeed);
  const setCurrentTimeS = useReplayStore((state) => state.setCurrentTimeS);
  const pause = useReplayStore((state) => state.pause);
  const loadReplayFromText = useReplayStore((state) => state.loadReplayFromText);
  const [selectedEntityId, setSelectedEntityId] = useState<string | null>(null);
  const [focusMode, setFocusMode] = useState(false);

  useEffect(() => {
    if (!dataset) {
      loadReplayFromText(sampleMissionJsonl);
    }
  }, [dataset, loadReplayFromText]);

  useEffect(() => {
    if (!dataset || !isPlaying) {
      return;
    }
    const id = window.setInterval(() => {
      const next = currentTimeS + 0.05 * playbackSpeed;
      if (next >= dataset.endTimeS) {
        setCurrentTimeS(dataset.endTimeS);
        pause();
        return;
      }
      setCurrentTimeS(next);
    }, 50);
    return () => window.clearInterval(id);
  }, [currentTimeS, dataset, isPlaying, pause, playbackSpeed, setCurrentTimeS]);

  const frame = useMemo(
    () => (dataset ? getReplayFrameAt(dataset, currentTimeS) : null),
    [currentTimeS, dataset]
  );

  if (!dataset || !frame) {
    return (
      <div className="rounded-2xl border border-line bg-panel/85 p-5 text-sm text-muted">
        Loading replay dataset.
      </div>
    );
  }

  const demoSummary = getDemoSummary(dataset);

  return (
    <section className="space-y-4">
      <div className="rounded-2xl border border-line bg-panel/85 p-5">
        <div className="mb-4 flex flex-col gap-2 lg:flex-row lg:items-end lg:justify-between">
          <div>
            <p className="text-xs uppercase tracking-[0.24em] text-signal">Mission Replay Console</p>
            <h2 className="mt-2 text-xl font-semibold text-white">
              Degraded Sensing Multi-Engagement Replay
            </h2>
            <p className="text-sm text-muted">
              Parsed C++ JSONL replay with tracks, interceptor state, assignments, BT decisions, and faults.
            </p>
          </div>
          <div className="flex flex-wrap items-center gap-2 lg:justify-end">
            <span className="rounded-md border border-signal/25 bg-signal/10 px-2 py-1 text-xs font-medium text-signal">
              Source: deterministic C++ JSONL replay
            </span>
            <button
              aria-pressed={focusMode}
              className={`rounded-md border px-2 py-1 text-xs font-semibold uppercase tracking-[0.14em] ${
                focusMode
                  ? "border-signal/40 bg-signal/15 text-signal"
                  : "border-white/10 bg-ink/45 text-muted"
              }`}
              onClick={() => setFocusMode((enabled) => !enabled)}
              type="button"
            >
              {focusMode ? "Exit Focus" : "Focus Mode"}
            </button>
          </div>
        </div>
        <DemoSummaryStrip items={demoSummary} />
        <p className="mt-3 text-xs text-muted">
          Parsed locally from simulator logs. No live backend required for this replay demo.
        </p>
        <div className="mt-4">
          <ReplayMetricsStrip dataset={dataset} frame={frame} />
        </div>
      </div>

      <div
        className={`grid gap-4 ${
          focusMode ? "xl:grid-cols-[minmax(0,1fr)_300px]" : "xl:grid-cols-[minmax(0,1fr)_340px]"
        }`}
      >
        <div className="space-y-4">
          <ReplayTacticalCanvas
            frame={frame}
            onSelectEntity={setSelectedEntityId}
            selectedEntityId={selectedEntityId}
          />
          <ReplayTimelineControls dataset={dataset} />
        </div>
        <div className="space-y-4">
          <ReplayBtPanel frame={frame} />
          {focusMode ? null : <ReplayEventFeed events={frame.recentEvents} />}
        </div>
      </div>
    </section>
  );
}

function DemoSummaryStrip({ items }: { items: Array<{ label: string; value: string }> }) {
  return (
    <div className="grid gap-2 sm:grid-cols-2 lg:grid-cols-4 2xl:grid-cols-7">
      {items.map((item) => (
        <div
          className="min-w-0 rounded-lg border border-line bg-ink/45 px-3 py-2"
          key={item.label}
        >
          <p className="truncate text-[9px] uppercase tracking-[0.16em] text-muted">{item.label}</p>
          <p className="mt-1 truncate font-mono text-xs font-semibold text-white" title={item.value}>
            {item.value}
          </p>
        </div>
      ))}
    </div>
  );
}

function getDemoSummary(dataset: ReplayDataset) {
  const trackCount = new Set(
    dataset.tracks
      .map((track) => track.track_id)
      .filter((trackId): trackId is number => trackId !== null)
  ).size;
  const interceptorCount = new Set(
    dataset.interceptors
      .map((interceptor) => interceptor.interceptor_id)
      .filter((interceptorId): interceptorId is number => interceptorId !== null)
  ).size;
  const successfulIntercepts = dataset.events.filter(
    (event) => event.type === "intercept_event" && event.outcome?.includes("success")
  ).length;

  return [
    { label: "Replay", value: `${dataset.durationS.toFixed(1)}s degraded-sensing replay` },
    { label: "Tracks", value: `${trackCount || 5} tracks` },
    { label: "Interceptors", value: `${interceptorCount || 3} interceptors` },
    { label: "Faults", value: `${dataset.faults.length} injected faults` },
    { label: "Intercepts", value: `${successfulIntercepts} intercepts` },
    { label: "Parse", value: `${dataset.errors.length} parse errors` },
    { label: "Format", value: "C++ JSONL replay" }
  ];
}
