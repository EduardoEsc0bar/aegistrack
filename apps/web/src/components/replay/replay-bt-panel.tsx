import { ReplayFrame } from "@/lib/replay/selectors";

type Props = {
  frame: ReplayFrame;
};

export function ReplayBtPanel({ frame }: Props) {
  const decision = frame.latestBtDecision;
  const currentMode = decision?.mode ?? "n/a";

  return (
    <div className="rounded-2xl border border-line bg-panel/85 p-4">
      <div className="mb-4 flex items-center justify-between">
        <div>
          <p className="text-xs uppercase tracking-[0.22em] text-muted">Behavior Tree</p>
          <p className="mt-1 text-xs text-muted">Behavior tree decision trace.</p>
        </div>
        <span
          className="max-w-[170px] truncate rounded-md border border-signal/30 bg-signal/10 px-2 py-1 text-xs font-semibold uppercase text-signal"
          title={currentMode}
        >
          {currentMode}
        </span>
      </div>
      {decision ? (
        <div className="space-y-3 text-sm">
          <ModeStrip currentMode={currentMode} />

          <div className="rounded-xl border border-white/5 bg-ink/45 p-3">
            <div className="mb-3 flex flex-wrap gap-2">
              <DecisionChip label="Track" value={decision.selected_track_id || "n/a"} />
              <DecisionChip label="Interceptor" value={decision.selected_interceptor_id || "n/a"} />
              <DecisionChip
                label="Score"
                value={decision.selected_engagement_score?.toFixed(2) ?? "n/a"}
              />
              <DecisionChip
                label="ETA"
                value={
                  decision.selected_estimated_intercept_time_s !== null
                    ? `${decision.selected_estimated_intercept_time_s.toFixed(2)}s`
                    : "n/a"
                }
              />
            </div>
            <div className="space-y-2 text-xs">
              <InfoRow label="Decision" value={decision.decision_type ?? "unknown"} />
              <InfoRow label="Reason" value={decision.reason ?? "n/a"} />
              <InfoRow label="Assignment" value={decision.assignment_reason ?? "n/a"} />
              <InfoRow label="Active" value={String(decision.active_engagements ?? 0)} mono />
            </div>
          </div>

          <div className="rounded-xl border border-white/5 bg-ink/50 p-3">
            <p className="mb-2 text-xs uppercase tracking-[0.18em] text-muted">Node Trace</p>
            <div className="space-y-2">
              {decision.nodes.slice(0, 5).map((node) => (
                <div
                  className="rounded-lg border border-white/5 bg-panel/35 px-2 py-2 text-xs"
                  key={`${node.node}-${node.detail}`}
                >
                  <div className="flex items-center justify-between gap-3">
                    <span className="min-w-0 truncate font-mono text-text" title={node.node}>
                      {node.node}
                    </span>
                    <StatusChip status={node.status} />
                  </div>
                  {node.detail ? (
                    <p className="mt-1 max-h-10 overflow-hidden break-words text-muted">
                      {node.detail}
                    </p>
                  ) : null}
                </div>
              ))}
            </div>
          </div>
        </div>
      ) : (
        <p className="text-sm text-muted">No BT decision at the current replay time.</p>
      )}
    </div>
  );
}

function ModeStrip({ currentMode }: { currentMode: string }) {
  const normalizedMode = currentMode.toLowerCase();
  const modes = [
    { label: "SEARCH", title: "Search", active: normalizedMode === "search" },
    { label: "TRACK", title: "Track", active: normalizedMode === "track" },
    {
      label: "ENGAGE",
      title: "Engage",
      active: normalizedMode === "engage" || normalizedMode === "idle_interceptor_engage"
    },
    {
      label: "MAINT",
      title: "Maintain",
      active: normalizedMode === "engage_maintain" || normalizedMode === "maintain"
    },
    { label: "RETASK", title: "Retask", active: normalizedMode.startsWith("retask") }
  ];

  return (
    <div className="grid min-w-0 grid-cols-5 overflow-hidden rounded-lg border border-line bg-ink/50">
      {modes.map((mode) => {
        return (
          <div
            className={`min-w-0 truncate border-r border-line/80 px-1 py-2 text-center text-[9px] font-semibold tracking-[0.06em] last:border-r-0 ${
              mode.active ? "bg-signal/15 text-signal" : "text-muted"
            }`}
            key={mode.label}
            title={mode.title}
          >
            {mode.label}
          </div>
        );
      })}
    </div>
  );
}

function DecisionChip({ label, value }: { label: string; value: number | string }) {
  return (
    <span className="min-w-[72px] rounded-md border border-line bg-panel/60 px-2 py-1">
      <span className="block text-[9px] uppercase tracking-[0.16em] text-muted">{label}</span>
      <span className="block truncate font-mono text-xs font-semibold text-white" title={String(value)}>
        {value}
      </span>
    </span>
  );
}

function InfoRow({ label, mono = false, value }: { label: string; mono?: boolean; value: string }) {
  return (
    <div className="grid grid-cols-[82px_minmax(0,1fr)] gap-2">
      <span className="text-muted">{label}</span>
      <span
        className={`min-w-0 break-words text-right text-white ${mono ? "font-mono" : ""}`}
        title={value}
      >
        {value}
      </span>
    </div>
  );
}

function StatusChip({ status }: { status: string }) {
  const normalized = status.toLowerCase();
  const classes = normalized === "success"
    ? "border-emerald-300/30 bg-emerald-300/10 text-emerald-300"
    : normalized === "failure"
      ? "border-danger/30 bg-danger/10 text-danger"
      : "border-warning/30 bg-warning/10 text-warning";

  return (
    <span className={`shrink-0 rounded px-1.5 py-0.5 text-[10px] font-semibold uppercase ${classes}`}>
      {status}
    </span>
  );
}
