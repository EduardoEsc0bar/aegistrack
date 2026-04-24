"use client";

import { useUiStore } from "@/store/ui-store";

const labels = {
  tracks: "Tracks",
  sensors: "Sensors",
  predictedPaths: "Predicted Paths",
  coverageCones: "Coverage Cones",
  alerts: "Alerts",
  heatmap: "Heatmap"
} as const;

export function LayerTogglePanel() {
  const layers = useUiStore((state) => state.layers);
  const toggleLayer = useUiStore((state) => state.toggleLayer);

  return (
    <div className="rounded-2xl border border-line bg-panel/90 p-4 shadow-panel">
      <p className="mb-3 text-xs uppercase tracking-[0.22em] text-muted">Layer Controls</p>
      <div className="grid gap-2">
        {Object.entries(labels).map(([key, label]) => (
          <button
            key={key}
            className={`flex items-center justify-between rounded-xl border px-3 py-2 text-sm ${
              layers[key as keyof typeof layers]
                ? "border-signal/30 bg-signal/10 text-white"
                : "border-white/5 bg-ink/60 text-muted"
            }`}
            onClick={() => toggleLayer(key as keyof typeof layers)}
          >
            <span>{label}</span>
            <span>{layers[key as keyof typeof layers] ? "ON" : "OFF"}</span>
          </button>
        ))}
      </div>
    </div>
  );
}
