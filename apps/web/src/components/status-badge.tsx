import { classNames } from "@/lib/utils";

type Props = {
  label: string;
  tone?: "neutral" | "warning" | "danger" | "success";
};

export function StatusBadge({ label, tone = "neutral" }: Props) {
  return (
    <span
      className={classNames(
        "inline-flex items-center rounded-full border px-2.5 py-1 text-[11px] font-semibold uppercase tracking-[0.18em]",
        tone === "success" && "border-emerald-500/30 bg-emerald-500/10 text-emerald-300",
        tone === "warning" && "border-warning/30 bg-warning/10 text-warning",
        tone === "danger" && "border-danger/30 bg-danger/10 text-danger",
        tone === "neutral" && "border-line bg-white/5 text-muted"
      )}
    >
      {label}
    </span>
  );
}
