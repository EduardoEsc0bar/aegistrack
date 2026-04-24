"use client";

import { AppShell } from "@/components/app-shell";
import { PlaybackControls } from "@/components/playback-controls";
import { StatusBadge } from "@/components/status-badge";
import { TimelineChart } from "@/components/timeline-chart";
import { useMissionControlData } from "@/hooks/use-mission-control-data";
import { formatUtc, severityTone } from "@/lib/utils";

export function TimelineView() {
  const { events, playback } = useMissionControlData();

  return (
    <AppShell
      title="Mission Timeline"
      subtitle="Playback-centric event stream adapted from AegisTrack track, decision, assignment, and intercept outputs."
    >
      <section className="grid gap-6 xl:grid-cols-[1fr_320px]">
        <div className="space-y-6">
          <TimelineChart events={events.data ?? []} />
          <div className="rounded-2xl border border-line bg-panel/85 p-5">
            <div className="mb-4 flex items-center justify-between">
              <h2 className="text-lg font-semibold text-white">Chronological Mission Events</h2>
              <p className="text-xs uppercase tracking-[0.18em] text-muted">severity, asset, type filters ready</p>
            </div>
            <div className="space-y-3">
              {(events.data ?? []).slice(0, 14).map((event) => (
                <div key={event.id} className="rounded-xl border border-white/5 bg-ink/50 p-4">
                  <div className="mb-2 flex items-center justify-between gap-3">
                    <p className="font-medium text-white">{event.title}</p>
                    <StatusBadge label={event.severity} tone={severityTone(event.severity)} />
                  </div>
                  <p className="text-sm text-muted">{event.description}</p>
                  <p className="mt-3 text-xs uppercase tracking-[0.18em] text-muted">{formatUtc(event.occurred_at)}</p>
                </div>
              ))}
            </div>
          </div>
        </div>
        <PlaybackControls playback={playback.data} />
      </section>
    </AppShell>
  );
}
