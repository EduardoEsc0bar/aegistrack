import { TelemetryUpdate } from "@/lib/types";

type Props = {
  telemetry: TelemetryUpdate[];
};

export function TelemetryPanel({ telemetry }: Props) {
  return (
    <div className="rounded-2xl border border-line bg-panel/85 p-5">
      <div className="mb-4 flex items-center justify-between">
        <h2 className="text-lg font-semibold text-white">Telemetry Stream</h2>
        <p className="text-xs uppercase tracking-[0.18em] text-muted">live packets</p>
      </div>
      <div className="space-y-3">
        {telemetry.slice(0, 5).map((packet) => (
          <div key={`${packet.asset_id}-${packet.timestamp}`} className="rounded-xl border border-white/5 bg-ink/40 p-4">
            <div className="flex items-center justify-between">
              <p className="font-medium text-white">{packet.asset_id}</p>
              <p className="text-sm text-signal">{packet.track.classification}</p>
            </div>
            <div className="mt-3 grid grid-cols-2 gap-2 text-sm text-muted">
              <span>Lat {packet.position.lat.toFixed(3)}</span>
              <span>Lon {packet.position.lon.toFixed(3)}</span>
              <span>Alt {Math.round(packet.position.alt)} m</span>
              <span>Speed {packet.velocity.speed_mps.toFixed(1)} m/s</span>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
