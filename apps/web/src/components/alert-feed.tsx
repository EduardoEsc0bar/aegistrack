import { StatusBadge } from "@/components/status-badge";
import { Event } from "@/lib/types";
import { formatUtc, severityTone } from "@/lib/utils";

type Props = {
  events: Event[];
};

export function AlertFeed({ events }: Props) {
  return (
    <div className="rounded-2xl border border-line bg-panel/85 p-5">
      <div className="mb-4 flex items-center justify-between">
        <h2 className="text-lg font-semibold text-white">Recent Alerts</h2>
        <StatusBadge label={`${events.length} active`} tone={events.length ? "warning" : "neutral"} />
      </div>
      <div className="space-y-3">
        {events.slice(0, 6).map((event) => (
          <div key={event.id} className="rounded-xl border border-white/5 bg-ink/40 p-4">
            <div className="mb-2 flex items-center justify-between gap-3">
              <p className="text-sm font-medium text-white">{event.title}</p>
              <StatusBadge label={event.severity} tone={severityTone(event.severity)} />
            </div>
            <p className="text-sm text-muted">{event.description}</p>
            <p className="mt-2 text-xs uppercase tracking-[0.18em] text-muted">
              {formatUtc(event.occurred_at)}
            </p>
          </div>
        ))}
      </div>
    </div>
  );
}
