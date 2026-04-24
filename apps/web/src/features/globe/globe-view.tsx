"use client";

import { AppShell } from "@/components/app-shell";
import { EntityDetailsPanel } from "@/components/entity-details-panel";
import { GlobeViewer } from "@/components/globe-viewer";
import { LayerTogglePanel } from "@/components/layer-toggle-panel";
import { PlaybackControls } from "@/components/playback-controls";
import { useMissionControlData } from "@/hooks/use-mission-control-data";
import { useTelemetrySocket } from "@/hooks/use-telemetry-socket";

export function GlobeView() {
  const { assets, sensors, tracks, playback } = useMissionControlData();
  const latestSocketTelemetry = useTelemetrySocket();

  return (
    <AppShell
      title="Geospatial Operations View"
      subtitle="Full-screen globe visualization of AegisTrack tracks, sensors, predicted paths, and operator overlays."
    >
      <section className="grid gap-6 xl:grid-cols-[1fr_320px]">
        <div className="space-y-4">
          <GlobeViewer assets={assets.data ?? []} sensors={sensors.data ?? []} tracks={tracks.data ?? []} />
          <div className="rounded-2xl border border-line bg-panel/85 px-5 py-4 text-sm text-muted">
            Timeline scrubber placeholder.
            {latestSocketTelemetry ? ` Live packet: ${latestSocketTelemetry.asset_id}` : " Waiting for telemetry."}
          </div>
        </div>
        <div className="space-y-4">
          <LayerTogglePanel />
          <EntityDetailsPanel assets={assets.data ?? []} tracks={tracks.data ?? []} />
          <PlaybackControls playback={playback.data} />
        </div>
      </section>
    </AppShell>
  );
}
