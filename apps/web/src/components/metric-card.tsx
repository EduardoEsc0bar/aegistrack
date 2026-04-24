import { ReactNode } from "react";

import { StatusBadge } from "@/components/status-badge";

type Props = {
  label: string;
  value: string | number;
  detail: string;
  badge?: { label: string; tone?: "neutral" | "warning" | "danger" | "success" };
  icon?: ReactNode;
};

export function MetricCard({ label, value, detail, badge, icon }: Props) {
  return (
    <div className="rounded-2xl border border-line bg-panel/85 p-5 shadow-panel">
      <div className="mb-5 flex items-start justify-between">
        <div>
          <p className="text-[11px] uppercase tracking-[0.24em] text-muted">{label}</p>
          <p className="mt-3 text-3xl font-semibold text-white">{value}</p>
        </div>
        <div className="rounded-xl border border-white/10 bg-white/5 p-3 text-signal">{icon}</div>
      </div>
      <div className="flex items-center justify-between">
        <p className="text-sm text-muted">{detail}</p>
        {badge ? <StatusBadge label={badge.label} tone={badge.tone} /> : null}
      </div>
    </div>
  );
}
