"use client";

import { ReplayDataset } from "@/lib/replay/types";
import { useReplayStore } from "@/store/replay-store";

type Props = {
  dataset: ReplayDataset | null;
};

export function ReplayTimelineControls({ dataset }: Props) {
  const currentTimeS = useReplayStore((state) => state.currentTimeS);
  const isPlaying = useReplayStore((state) => state.isPlaying);
  const playbackSpeed = useReplayStore((state) => state.playbackSpeed);
  const setCurrentTimeS = useReplayStore((state) => state.setCurrentTimeS);
  const play = useReplayStore((state) => state.play);
  const pause = useReplayStore((state) => state.pause);
  const setPlaybackSpeed = useReplayStore((state) => state.setPlaybackSpeed);
  const start = dataset?.startTimeS ?? 0;
  const end = dataset?.endTimeS ?? 0;
  const duration = dataset?.durationS ?? 0;

  return (
    <div className="rounded-2xl border border-line bg-panel/85 p-4">
      <div className="mb-3 flex items-center justify-between gap-3">
        <div>
          <p className="text-xs uppercase tracking-[0.2em] text-muted">Replay Timeline</p>
          <p className="text-xs text-muted">Scrub replay time from parsed JSONL events.</p>
          <p className="font-mono text-sm text-white">
            {currentTimeS.toFixed(2)}s / {duration.toFixed(2)}s
          </p>
        </div>
        <button
          className="rounded-lg border border-signal/30 bg-signal/10 px-3 py-2 text-xs font-semibold uppercase tracking-[0.16em] text-signal"
          onClick={isPlaying ? pause : play}
          type="button"
        >
          {isPlaying ? "Pause" : "Play"}
        </button>
      </div>
      <input
        aria-label="Replay time"
        className="w-full accent-[#4ad7d1]"
        disabled={!dataset}
        max={end}
        min={start}
        onChange={(event) => setCurrentTimeS(Number(event.target.value))}
        step="0.01"
        type="range"
        value={currentTimeS}
      />
      <div className="mt-3 flex items-center justify-between text-xs text-muted">
        <span className="font-mono">{start.toFixed(2)}s</span>
        <div className="flex items-center gap-2">
          <span>Speed</span>
          {[0.5, 1, 2, 4].map((speed) => (
            <button
              className={`rounded-md border px-2 py-1 font-mono ${
                playbackSpeed === speed
                  ? "border-signal/30 bg-signal/10 text-white"
                  : "border-white/10 bg-ink/50 text-muted"
              }`}
              key={speed}
              onClick={() => setPlaybackSpeed(speed)}
              type="button"
            >
              {speed}x
            </button>
          ))}
        </div>
        <span className="font-mono">{end.toFixed(2)}s</span>
      </div>
    </div>
  );
}
