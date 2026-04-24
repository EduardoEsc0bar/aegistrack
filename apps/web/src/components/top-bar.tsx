import { ConnectionIndicator } from "@/components/connection-indicator";
import { StatusBadge } from "@/components/status-badge";

type Props = {
  title: string;
  subtitle: string;
};

export function TopBar({ title, subtitle }: Props) {
  return (
    <header className="border-b border-line bg-[#0a1624]/80 px-6 py-5 backdrop-blur">
      <div className="flex flex-col gap-4 lg:flex-row lg:items-center lg:justify-between">
        <div>
          <h1 className="text-2xl font-semibold text-white">{title}</h1>
          <p className="text-sm text-muted">{subtitle}</p>
        </div>
        <div className="flex items-center gap-3">
          <StatusBadge label="Mission Ops Layer" tone="neutral" />
          <ConnectionIndicator />
        </div>
      </div>
    </header>
  );
}
