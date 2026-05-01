"use client";

import { ReplayFrame, ReplayFrameInterceptor, ReplayFrameTrack } from "@/lib/replay/selectors";

type Props = {
  frame: ReplayFrame;
  selectedEntityId: string | null;
  onSelectEntity: (entityId: string) => void;
};

const width = 920;
const height = 520;
const padding = 52;

export function ReplayTacticalCanvas({ frame, selectedEntityId, onSelectEntity }: Props) {
  const bounds = getBounds(frame);
  const assignedTrackIds = new Set(frame.assignments.map((assignment) => assignment.trackId));
  const assignedInterceptorIds = new Set(
    frame.assignments.map((assignment) => assignment.interceptorId)
  );
  const interceptedTrackIds = new Set(
    frame.intercepts
      .filter((intercept) => intercept.outcome?.includes("success"))
      .map((intercept) => intercept.track_id)
      .filter((trackId): trackId is number => trackId !== null)
  );
  const project = (x: number | null, y: number | null) => {
    if (x === null || y === null) {
      return null;
    }
    const spanX = Math.max(1, bounds.maxX - bounds.minX);
    const spanY = Math.max(1, bounds.maxY - bounds.minY);
    return {
      x: padding + ((x - bounds.minX) / spanX) * (width - padding * 2),
      y: height - padding - ((y - bounds.minY) / spanY) * (height - padding * 2)
    };
  };

  return (
    <div className="overflow-hidden rounded-2xl border border-line bg-[#07111d] shadow-panel">
      <div className="flex flex-col gap-3 border-b border-line bg-panel/55 px-4 py-3 lg:flex-row lg:items-center lg:justify-between">
        <div>
          <p className="text-xs uppercase tracking-[0.22em] text-muted">Tactical Replay Snapshot</p>
          <p className="font-mono text-sm text-white">t={frame.timeS.toFixed(2)}s / local x-y plane</p>
          <p className="text-xs text-muted">Local x-y replay plane.</p>
        </div>
        <div className="flex flex-wrap gap-2 text-xs text-muted">
          <LegendItem color="#ffb65c" label="Track" marker="●" />
          <LegendItem color="#4ad7d1" label="Interceptor" marker="▲" />
          <LegendItem color="#8fb3ff" label="Assignment" marker="━" />
          <LegendItem color="#76e4a6" label="Intercept" marker="◆" />
        </div>
      </div>
      <svg className="h-[520px] w-full" role="img" viewBox={`0 0 ${width} ${height}`}>
        <defs>
          <radialGradient cx="50%" cy="45%" id="replayCanvasGlow" r="72%">
            <stop offset="0%" stopColor="#102a43" stopOpacity="0.75" />
            <stop offset="58%" stopColor="#07111d" stopOpacity="0.95" />
            <stop offset="100%" stopColor="#050b13" stopOpacity="1" />
          </radialGradient>
          <filter id="softGlow" x="-50%" y="-50%" width="200%" height="200%">
            <feGaussianBlur stdDeviation="3" result="blur" />
            <feMerge>
              <feMergeNode in="blur" />
              <feMergeNode in="SourceGraphic" />
            </feMerge>
          </filter>
        </defs>
        <rect fill="url(#replayCanvasGlow)" height={height} width={width} />
        <Grid bounds={bounds} />
        {frame.assignments.map((assignment) => {
          const track = frame.tracks.find((item) => item.trackId === assignment.trackId);
          const interceptor = frame.interceptors.find(
            (item) => item.interceptorId === assignment.interceptorId
          );
          const trackPoint = project(track?.x ?? null, track?.y ?? null);
          const interceptorPoint = project(interceptor?.x ?? null, interceptor?.y ?? null);
          if (!trackPoint || !interceptorPoint) {
            return null;
          }
          const midX = (trackPoint.x + interceptorPoint.x) / 2;
          const midY = (trackPoint.y + interceptorPoint.y) / 2;
          return (
            <g key={`${assignment.interceptorId}-${assignment.trackId}`}>
              <line
                stroke="#07111d"
                strokeLinecap="round"
                strokeOpacity="0.85"
                strokeWidth="6"
                x1={interceptorPoint.x}
                x2={trackPoint.x}
                y1={interceptorPoint.y}
                y2={trackPoint.y}
              />
              <line
                stroke="#8fb3ff"
                strokeDasharray="8 6"
                strokeLinecap="round"
                strokeOpacity="0.9"
                strokeWidth="2"
                x1={interceptorPoint.x}
                x2={trackPoint.x}
                y1={interceptorPoint.y}
                y2={trackPoint.y}
              />
              <circle cx={midX} cy={midY} fill="#8fb3ff" opacity="0.9" r="2.8" />
            </g>
          );
        })}
        {frame.tracks.map((track) => (
          <TrackGlyph
            key={track.trackId}
            assigned={assignedTrackIds.has(track.trackId)}
            intercepted={interceptedTrackIds.has(track.trackId)}
            onSelectEntity={onSelectEntity}
            point={project(track.x, track.y)}
            project={project}
            selected={selectedEntityId === `track-${track.trackId}`}
            track={track}
          />
        ))}
        {frame.interceptors.map((interceptor) => (
          <InterceptorGlyph
            assigned={assignedInterceptorIds.has(interceptor.interceptorId)}
            interceptor={interceptor}
            key={interceptor.interceptorId}
            onSelectEntity={onSelectEntity}
            point={project(interceptor.x, interceptor.y)}
            project={project}
            selected={selectedEntityId === `interceptor-${interceptor.interceptorId}`}
          />
        ))}
      </svg>
    </div>
  );
}

function LegendItem({ color, label, marker }: { color: string; label: string; marker: string }) {
  return (
    <span className="rounded-md border border-white/10 bg-ink/45 px-2 py-1">
      <span style={{ color }}>{marker}</span> {label}
    </span>
  );
}

function Grid({ bounds }: { bounds: ReturnType<typeof getBounds> }) {
  const lines = Array.from({ length: 9 }, (_, index) => index);
  return (
    <g>
      {lines.map((index) => {
        const x = padding + (index / 8) * (width - padding * 2);
        const y = padding + (index / 8) * (height - padding * 2);
        const isCenter = index === 4;
        return (
          <g key={index}>
            <line
              stroke={isCenter ? "#34506b" : "#1d3145"}
              strokeOpacity={isCenter ? "0.95" : "0.72"}
              strokeWidth={isCenter ? "1.5" : "1"}
              x1={x}
              x2={x}
              y1={padding}
              y2={height - padding}
            />
            <line
              stroke={isCenter ? "#34506b" : "#1d3145"}
              strokeOpacity={isCenter ? "0.95" : "0.72"}
              strokeWidth={isCenter ? "1.5" : "1"}
              x1={padding}
              x2={width - padding}
              y1={y}
              y2={y}
            />
          </g>
        );
      })}
      <rect
        fill="none"
        height={height - padding * 2}
        stroke="#284057"
        strokeWidth="1.5"
        width={width - padding * 2}
        x={padding}
        y={padding}
      />
      <text fill="#5f7891" fontFamily="ui-monospace, monospace" fontSize="10" x={padding} y={height - 18}>
        X {Math.round(bounds.minX)}m
      </text>
      <text fill="#5f7891" fontFamily="ui-monospace, monospace" fontSize="10" textAnchor="end" x={width - padding} y={height - 18}>
        X {Math.round(bounds.maxX)}m
      </text>
      <text fill="#5f7891" fontFamily="ui-monospace, monospace" fontSize="10" x={16} y={padding + 4}>
        Y {Math.round(bounds.maxY)}m
      </text>
      <text fill="#5f7891" fontFamily="ui-monospace, monospace" fontSize="10" x={16} y={height - padding}>
        Y {Math.round(bounds.minY)}m
      </text>
    </g>
  );
}

function TrackGlyph({
  assigned,
  intercepted,
  onSelectEntity,
  point,
  project,
  selected,
  track
}: {
  assigned: boolean;
  intercepted: boolean;
  onSelectEntity: (entityId: string) => void;
  point: { x: number; y: number } | null;
  project: (x: number | null, y: number | null) => { x: number; y: number } | null;
  selected: boolean;
  track: ReplayFrameTrack;
}) {
  if (!point) {
    return null;
  }
  const trail = track.trail.map((item) => project(item.x, item.y)).filter(Boolean) as Array<{
    x: number;
    y: number;
  }>;
  const status = trackStatus(track, intercepted);
  const markerRadius = selected ? 9 : assigned ? 7.5 : 6.5;
  return (
    <g
      aria-label={`Track ${track.trackId}`}
      className="cursor-pointer"
      onKeyDown={(event) => {
        if (event.key === "Enter" || event.key === " ") {
          event.preventDefault();
          onSelectEntity(`track-${track.trackId}`);
        }
      }}
      onClick={() => onSelectEntity(`track-${track.trackId}`)}
      role="button"
      tabIndex={0}
    >
      {trail.length > 1 ? (
        <g>
          <polyline
            fill="none"
            points={trail.map((item) => `${item.x},${item.y}`).join(" ")}
            stroke="#07111d"
            strokeLinecap="round"
            strokeLinejoin="round"
            strokeOpacity="0.9"
            strokeWidth="6"
          />
          <polyline
            fill="none"
            points={trail.map((item) => `${item.x},${item.y}`).join(" ")}
            stroke="#ffb65c"
            strokeLinecap="round"
            strokeLinejoin="round"
            strokeOpacity="0.55"
            strokeWidth="2.4"
          />
        </g>
      ) : null}
      {assigned ? (
        <circle
          cx={point.x}
          cy={point.y}
          fill="none"
          filter="url(#softGlow)"
          r="14"
          stroke="#ffb65c"
          strokeOpacity="0.45"
          strokeWidth="2"
        />
      ) : null}
      <circle
        cx={point.x}
        cy={point.y}
        fill={intercepted ? "#76e4a6" : "#ffb65c"}
        r={markerRadius}
        stroke={selected ? "#ffffff" : "#07111d"}
        strokeWidth="2.5"
      />
      <EntityLabel
        color={intercepted ? "#76e4a6" : status.color}
        label={`T-${track.trackId}`}
        point={point}
        status={status.label}
        xOffset={12}
        yOffset={-28}
      />
    </g>
  );
}

function InterceptorGlyph({
  assigned,
  interceptor,
  onSelectEntity,
  point,
  project,
  selected
}: {
  assigned: boolean;
  interceptor: ReplayFrameInterceptor;
  onSelectEntity: (entityId: string) => void;
  point: { x: number; y: number } | null;
  project: (x: number | null, y: number | null) => { x: number; y: number } | null;
  selected: boolean;
}) {
  if (!point) {
    return null;
  }
  const trail = interceptor.trail.map((item) => project(item.x, item.y)).filter(Boolean) as Array<{
    x: number;
    y: number;
  }>;
  const size = selected ? 12 : assigned ? 10 : 9;
  const color = interceptor.engaged ? "#4ad7d1" : "#8ea5bb";
  return (
    <g
      aria-label={`Interceptor ${interceptor.interceptorId}`}
      className="cursor-pointer"
      onKeyDown={(event) => {
        if (event.key === "Enter" || event.key === " ") {
          event.preventDefault();
          onSelectEntity(`interceptor-${interceptor.interceptorId}`);
        }
      }}
      onClick={() => onSelectEntity(`interceptor-${interceptor.interceptorId}`)}
      role="button"
      tabIndex={0}
    >
      {trail.length > 1 ? (
        <g>
          <polyline
            fill="none"
            points={trail.map((item) => `${item.x},${item.y}`).join(" ")}
            stroke="#07111d"
            strokeLinecap="round"
            strokeLinejoin="round"
            strokeOpacity="0.9"
            strokeWidth="6"
          />
          <polyline
            fill="none"
            points={trail.map((item) => `${item.x},${item.y}`).join(" ")}
            stroke={color}
            strokeLinecap="round"
            strokeLinejoin="round"
            strokeOpacity="0.58"
            strokeWidth="2.5"
          />
        </g>
      ) : null}
      {interceptor.engaged ? (
        <circle
          cx={point.x}
          cy={point.y}
          fill="none"
          filter="url(#softGlow)"
          r="15"
          stroke="#4ad7d1"
          strokeOpacity={assigned ? "0.48" : "0.25"}
          strokeWidth="2"
        />
      ) : null}
      <polygon
        fill={color}
        points={`${point.x},${point.y - size} ${point.x + size},${point.y + size} ${point.x - size},${point.y + size}`}
        stroke={selected ? "#ffffff" : "#07111d"}
        strokeWidth="2.5"
      />
      <EntityLabel
        color={color}
        label={`I-${interceptor.interceptorId}`}
        point={point}
        status={interceptor.engaged ? "engaged" : "idle"}
        xOffset={13}
        yOffset={14}
      />
    </g>
  );
}

function EntityLabel({
  color,
  label,
  point,
  status,
  xOffset,
  yOffset
}: {
  color: string;
  label: string;
  point: { x: number; y: number };
  status: string;
  xOffset: number;
  yOffset: number;
}) {
  const labelWidth = Math.max(70, label.length * 8 + status.length * 6 + 22);
  const x = Math.min(width - padding - labelWidth, Math.max(padding, point.x + xOffset));
  const y = Math.min(height - padding - 24, Math.max(padding, point.y + yOffset));

  return (
    <g>
      <rect
        fill="#07111d"
        fillOpacity="0.88"
        height="24"
        rx="5"
        stroke="#284057"
        width={labelWidth}
        x={x}
        y={y}
      />
      <circle cx={x + 10} cy={y + 12} fill={color} r="3" />
      <text fill="#dbe8f5" fontFamily="ui-monospace, monospace" fontSize="11" fontWeight="700" x={x + 18} y={y + 15}>
        {label}
      </text>
      <text fill={color} fontFamily="ui-sans-serif, system-ui, sans-serif" fontSize="9" fontWeight="700" textAnchor="end" x={x + labelWidth - 8} y={y + 15}>
        {status.toUpperCase()}
      </text>
    </g>
  );
}

function trackStatus(track: ReplayFrameTrack, intercepted: boolean): { label: string; color: string } {
  const stabilityEvent = track.latestStabilityEvent?.event?.toLowerCase() ?? "";
  const reason = track.latestStabilityEvent?.reason?.toLowerCase() ?? "";
  if (intercepted || reason.includes("intercept")) {
    return { label: "intercepted", color: "#76e4a6" };
  }
  if (stabilityEvent.includes("coast") || reason.includes("coast")) {
    return { label: "coasting", color: "#ffb65c" };
  }
  if (stabilityEvent.includes("stable") || reason.includes("fragmentation")) {
    return { label: "stable", color: "#8fb3ff" };
  }
  if (track.status?.toLowerCase() === "confirmed") {
    return { label: "confirmed", color: "#ffb65c" };
  }
  return { label: track.status?.toLowerCase() ?? "track", color: "#8ea5bb" };
}

function getBounds(frame: ReplayFrame) {
  const points = [
    ...frame.tracks.flatMap((track) => [
      track.x !== null && track.y !== null ? { x: track.x, y: track.y } : null,
      ...track.trail.map((point) => ({ x: point.x, y: point.y }))
    ]),
    ...frame.interceptors.flatMap((interceptor) => [
      interceptor.x !== null && interceptor.y !== null ? { x: interceptor.x, y: interceptor.y } : null,
      ...interceptor.trail.map((point) => ({ x: point.x, y: point.y }))
    ])
  ].filter((point): point is { x: number; y: number } => point !== null);
  if (points.length === 0) {
    return { minX: -1000, maxX: 1000, minY: -1000, maxY: 1000 };
  }
  const minX = Math.min(...points.map((point) => point.x));
  const maxX = Math.max(...points.map((point) => point.x));
  const minY = Math.min(...points.map((point) => point.y));
  const maxY = Math.max(...points.map((point) => point.y));
  const span = Math.max(maxX - minX, maxY - minY, 500);
  const centerX = (minX + maxX) / 2;
  const centerY = (minY + maxY) / 2;
  return {
    minX: centerX - span / 2,
    maxX: centerX + span / 2,
    minY: centerY - span / 2,
    maxY: centerY + span / 2
  };
}
