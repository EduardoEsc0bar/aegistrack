"use client";

import { useState } from "react";
import { useQueryClient } from "@tanstack/react-query";

import { StatusBadge } from "@/components/status-badge";
import { api } from "@/lib/api";
import { PlaybackState } from "@/lib/types";
import { formatUtc } from "@/lib/utils";
import { useUiStore } from "@/store/ui-store";

type Props = {
  playback: PlaybackState | undefined;
};

export function PlaybackControls({ playback }: Props) {
  const [busy, setBusy] = useState(false);
  const queryClient = useQueryClient();
  const setPlaybackMode = useUiStore((state) => state.setPlaybackMode);

  async function update(mode: "live" | "playback") {
    setBusy(true);
    if (mode === "playback") {
      await api.startPlayback();
    } else {
      await api.stopPlayback();
    }
    setPlaybackMode(mode);
    await queryClient.invalidateQueries({ queryKey: ["playback"] });
    setBusy(false);
  }

  return (
    <div className="rounded-2xl border border-line bg-panel/85 p-5">
      <div className="mb-4 flex items-center justify-between">
        <h2 className="text-lg font-semibold text-white">Playback Control</h2>
        <StatusBadge label={playback?.mode ?? "live"} tone={playback?.mode === "live" ? "success" : "warning"} />
      </div>
      <div className="mb-4 flex gap-3">
        <button
          className="rounded-xl border border-signal/30 bg-signal/10 px-4 py-2 text-sm font-medium text-signal"
          onClick={() => update("live")}
          disabled={busy}
        >
          LIVE
        </button>
        <button
          className="rounded-xl border border-warning/30 bg-warning/10 px-4 py-2 text-sm font-medium text-warning"
          onClick={() => update("playback")}
          disabled={busy}
        >
          PLAYBACK
        </button>
      </div>
      <div className="space-y-2 text-sm text-muted">
        <p>Source: {playback?.source ?? "Unavailable"}</p>
        <p>Current: {formatUtc(playback?.current_time ?? null)}</p>
        <p>Window Start: {formatUtc(playback?.start_time ?? null)}</p>
      </div>
    </div>
  );
}
