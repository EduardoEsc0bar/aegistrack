"use client";

import { Asset, Track } from "@/lib/types";
import { formatUtc } from "@/lib/utils";
import { useUiStore } from "@/store/ui-store";

type Props = {
  assets: Asset[];
  tracks: Track[];
};

export function EntityDetailsPanel({ assets, tracks }: Props) {
  const selectedEntityId = useUiStore((state) => state.selectedEntityId);
  const entity = assets.find((item) => item.id === selectedEntityId);
  const track = tracks.find((item) => item.asset_id === selectedEntityId || item.id === selectedEntityId);

  return (
    <div className="rounded-2xl border border-line bg-panel/90 p-4 shadow-panel">
      <p className="mb-3 text-xs uppercase tracking-[0.22em] text-muted">Entity Details</p>
      {entity ? (
        <div className="space-y-3 text-sm">
          <div>
            <p className="text-lg font-semibold text-white">{entity.name}</p>
            <p className="text-muted">{entity.type}</p>
          </div>
          <div className="grid grid-cols-2 gap-2 text-muted">
            <span>Status</span>
            <span className="text-right text-white">{entity.status}</span>
            <span>Position</span>
            <span className="text-right text-white">
              {entity.latitude.toFixed(2)}, {entity.longitude.toFixed(2)}
            </span>
            <span>Altitude</span>
            <span className="text-right text-white">{Math.round(entity.altitude)} m</span>
            <span>Speed</span>
            <span className="text-right text-white">{entity.speed.toFixed(1)} m/s</span>
          </div>
          {track ? (
            <div className="rounded-xl border border-white/5 bg-ink/50 p-3 text-muted">
              <p className="font-medium text-white">Track confidence {Math.round(track.confidence * 100)}%</p>
              <p>Last update {formatUtc(track.last_update_at)}</p>
            </div>
          ) : null}
        </div>
      ) : (
        <p className="text-sm text-muted">Select a track or asset from the globe or table to inspect its current state.</p>
      )}
    </div>
  );
}
