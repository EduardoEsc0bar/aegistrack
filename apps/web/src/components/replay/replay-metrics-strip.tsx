import { ReplayFrame } from "@/lib/replay/selectors";
import { ReplayDataset } from "@/lib/replay/types";

type Props = {
  dataset: ReplayDataset;
  frame: ReplayFrame;
};

export function ReplayMetricsStrip({ dataset, frame }: Props) {
  const values = [
    { label: "Tracks", value: frame.tracks.length, tone: "text-warning" },
    { label: "Interceptors", value: frame.interceptors.length, tone: "text-signal" },
    { label: "Assignments", value: frame.assignments.length, tone: "text-[#8fb3ff]" },
    { label: "BT Mode", value: frame.latestBtDecision?.mode ?? "search", tone: "text-white" },
    { label: "Faults", value: dataset.faults.length, tone: "text-danger" },
    { label: "Intercepts", value: frame.intercepts.length, tone: "text-emerald-300" }
  ];

  return (
    <div className="grid gap-2 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-6">
      {values.map((item) => (
        <div
          className="min-w-0 rounded-lg border border-line bg-panel/70 px-3 py-2 shadow-[inset_0_1px_0_rgba(255,255,255,0.03)]"
          key={item.label}
        >
          <p className="truncate text-[10px] uppercase tracking-[0.18em] text-muted">
            {item.label}
          </p>
          <p
            className={`mt-1 truncate font-mono text-base font-semibold leading-6 ${item.tone}`}
            title={String(item.value)}
          >
            {item.value}
          </p>
        </div>
      ))}
    </div>
  );
}
