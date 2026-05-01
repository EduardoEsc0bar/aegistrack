import { ReplayEvent } from "@/lib/replay/types";

type Props = {
  events: ReplayEvent[];
};

export function ReplayEventFeed({ events }: Props) {
  return (
    <div className="rounded-2xl border border-line bg-panel/85 p-4">
      <div className="mb-4 flex items-center justify-between">
        <div>
          <p className="text-xs uppercase tracking-[0.22em] text-muted">Mission Events</p>
          <p className="mt-1 text-xs text-muted">Deterministic mission event stream.</p>
        </div>
        <span className="font-mono text-xs text-muted">{events.length}</span>
      </div>
      <div className="space-y-2">
        {events.map((event) => {
          const tone = eventTone(event);
          return (
            <div
              className={`rounded-xl border bg-ink/50 p-3 ${tone.card}`}
              key={event.eventId}
            >
              <div className="mb-2 flex items-start justify-between gap-3">
                <div className="flex min-w-0 items-center gap-2">
                  <TypeChip event={event} />
                  <p className="min-w-0 truncate text-sm font-medium text-white" title={eventLabel(event)}>
                    {eventLabel(event)}
                  </p>
                </div>
                <span className="shrink-0 rounded border border-line bg-panel/50 px-1.5 py-0.5 font-mono text-[11px] text-muted">
                  t+{event.t_s?.toFixed(2) ?? "n/a"}s
                </span>
              </div>
              <p className="break-words text-xs leading-5 text-muted">{eventDetail(event)}</p>
            </div>
          );
        })}
        {events.length === 0 ? (
          <p className="rounded-xl border border-white/5 bg-ink/40 p-3 text-sm text-muted">
            No mission events at this replay time.
          </p>
        ) : null}
      </div>
    </div>
  );
}

function eventLabel(event: ReplayEvent): string {
  if (event.type === "bt_decision") return `BT ${event.mode ?? "decision"}`;
  if (event.type === "assignment_event") return "Assignment";
  if (event.type === "intercept_event") return event.outcome?.includes("success") ? "Intercept Success" : "Intercept";
  if (event.type === "fault_event") return "Fault Injected";
  if (event.type === "track_stability_event") return event.event ?? "Track Stability";
  return event.type;
}

function eventDetail(event: ReplayEvent): string {
  if (event.type === "bt_decision") return `${event.decision_type ?? "decision"} / ${event.reason ?? "n/a"}`;
  if (event.type === "assignment_event") {
    return `I-${event.interceptor_id ?? "?"} -> T-${event.track_id ?? "?"} / ${event.reason ?? "n/a"}`;
  }
  if (event.type === "intercept_event") return `${event.outcome ?? "intercept"} / ${event.reason ?? "n/a"}`;
  if (event.type === "fault_event") return event.fault ?? "fault injected";
  if (event.type === "track_stability_event") {
    return `T-${event.track_id ?? "?"} / ${event.reason ?? "n/a"}`;
  }
  return "Replay event";
}

function TypeChip({ event }: { event: ReplayEvent }) {
  const chip = chipMeta(event);
  return (
    <span className={`shrink-0 rounded px-1.5 py-0.5 text-[10px] font-semibold uppercase ${chip.className}`}>
      {chip.label}
    </span>
  );
}

function chipMeta(event: ReplayEvent): { label: string; className: string } {
  if (event.type === "bt_decision") {
    const isRetask = event.decision_type?.includes("retask");
    return {
      label: "BT",
      className: isRetask
        ? "border border-warning/30 bg-warning/10 text-warning"
        : "border border-signal/30 bg-signal/10 text-signal"
    };
  }
  if (event.type === "fault_event") {
    return { label: "Fault", className: "border border-danger/30 bg-danger/10 text-danger" };
  }
  if (event.type === "assignment_event") {
    return { label: "Assign", className: "border border-[#8fb3ff]/30 bg-[#8fb3ff]/10 text-[#a9c0ff]" };
  }
  if (event.type === "intercept_event") {
    return { label: "Hit", className: "border border-emerald-300/30 bg-emerald-300/10 text-emerald-300" };
  }
  if (event.type === "track_stability_event") {
    return { label: "Track", className: "border border-warning/30 bg-warning/10 text-warning" };
  }
  return { label: "Event", className: "border border-line bg-panel/50 text-muted" };
}

function eventTone(event: ReplayEvent): { card: string } {
  if (event.type === "intercept_event" && event.outcome?.includes("success")) {
    return { card: "border-emerald-300/20 shadow-[inset_3px_0_0_rgba(110,231,183,0.45)]" };
  }
  if (event.type === "fault_event") {
    return { card: "border-danger/20 shadow-[inset_3px_0_0_rgba(255,107,107,0.45)]" };
  }
  if (event.type === "bt_decision" && event.decision_type?.includes("retask")) {
    return { card: "border-warning/20 shadow-[inset_3px_0_0_rgba(255,182,92,0.45)]" };
  }
  if (event.type === "bt_decision" && event.decision_type?.includes("engage")) {
    return { card: "border-signal/20 shadow-[inset_3px_0_0_rgba(74,215,209,0.4)]" };
  }
  return { card: "border-white/5" };
}
