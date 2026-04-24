"use client";

import { AppShell } from "@/components/app-shell";
import { ConnectionIndicator } from "@/components/connection-indicator";
import { StatusBadge } from "@/components/status-badge";
import { useMissionControlData } from "@/hooks/use-mission-control-data";

const API_BASE_URL = process.env.NEXT_PUBLIC_API_BASE_URL ?? "http://localhost:8000";
const WS_BASE_URL = process.env.NEXT_PUBLIC_WS_BASE_URL ?? "ws://localhost:8000";

export function SettingsView() {
  const { health, playback } = useMissionControlData();

  return (
    <AppShell
      title="Settings And Connection"
      subtitle="Environment visibility for the mission-control layer, including bridge health and live/playback mode."
    >
      <section className="grid gap-6 lg:grid-cols-2">
        <div className="rounded-2xl border border-line bg-panel/85 p-5">
          <h2 className="mb-4 text-lg font-semibold text-white">Connection Settings</h2>
          <div className="space-y-3 text-sm text-muted">
            <p>API Base URL: {API_BASE_URL}</p>
            <p>WebSocket Base URL: {WS_BASE_URL}</p>
            <p>Bridge Log Path: {String(health.data?.bridge?.log_path ?? "Unavailable")}</p>
            <div className="pt-2">
              <ConnectionIndicator />
            </div>
          </div>
        </div>
        <div className="rounded-2xl border border-line bg-panel/85 p-5">
          <h2 className="mb-4 text-lg font-semibold text-white">Runtime Mode</h2>
          <div className="space-y-3 text-sm text-muted">
            <p>Playback source: {playback.data?.source ?? "Unavailable"}</p>
            <p>Mode toggle is available in Dashboard, Globe, and Timeline views.</p>
            <div className="flex gap-3">
              <StatusBadge label={playback.data?.mode ?? "live"} tone={playback.data?.mode === "live" ? "success" : "warning"} />
              <StatusBadge label="Theme placeholder" tone="neutral" />
            </div>
          </div>
        </div>
      </section>
    </AppShell>
  );
}
