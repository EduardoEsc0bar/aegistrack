"use client";

import { AlertFeed } from "@/components/alert-feed";
import { AppShell } from "@/components/app-shell";
import { MetricCard } from "@/components/metric-card";
import { PlaybackControls } from "@/components/playback-controls";
import { TelemetryPanel } from "@/components/telemetry-panel";
import { useMissionControlData } from "@/hooks/use-mission-control-data";

export function DashboardView() {
  const { assets, sensors, tracks, events, telemetry, playback, health } = useMissionControlData();

  return (
    <AppShell
      title="Operator Dashboard"
      subtitle="Mission-operations summary layered over live AegisTrack simulation, replay, and telemetry."
    >
      <section className="grid gap-4 xl:grid-cols-4">
        <MetricCard label="Active Tracks" value={tracks.data?.length ?? 0} detail="Confirmed and tentative track states in the current picture." />
        <MetricCard label="Active Sensors" value={sensors.data?.filter((sensor) => sensor.is_active).length ?? 0} detail="Radar and EO/IR nodes currently feeding the mission layer." />
        <MetricCard label="Alerts" value={events.data?.filter((event) => event.severity !== "info").length ?? 0} detail="Warnings and critical decision outputs derived from AegisTrack events." />
        <MetricCard label="Messages" value={events.data?.length ?? 0} detail={`Bridge status: ${health.data?.status ?? "unknown"}.`} />
      </section>

      <section className="mt-6 grid gap-6 xl:grid-cols-[1.15fr_0.85fr]">
        <div className="grid gap-6">
          <TelemetryPanel telemetry={telemetry.data ?? []} />
          <AlertFeed events={events.data?.filter((event) => event.severity !== "info") ?? []} />
        </div>
        <div className="grid gap-6">
          <PlaybackControls playback={playback.data} />
          <div className="rounded-2xl border border-line bg-panel/85 p-5">
            <h2 className="mb-4 text-lg font-semibold text-white">System Health</h2>
            <div className="space-y-3 text-sm text-muted">
              <p>Service: {health.data?.service ?? "Unavailable"}</p>
              <p>Runtime assets: {assets.data?.length ?? 0}</p>
              <p>Telemetry packets: {telemetry.data?.length ?? 0}</p>
              <p>Bridge source: {String(health.data?.bridge?.log_path ?? "n/a")}</p>
            </div>
          </div>
        </div>
      </section>
    </AppShell>
  );
}
