import {
  AssignmentReplayEvent,
  BtDecisionReplayEvent,
  FaultReplayEvent,
  InterceptReplayEvent,
  InterceptorStateReplayEvent,
  ReplayDataset,
  ReplayEvent,
  TrackReplayEvent,
  TrackStabilityReplayEvent
} from "./types";

export type ReplayFrameTrack = {
  trackId: number;
  status: string | null;
  x: number | null;
  y: number | null;
  z: number | null;
  confidence: number | null;
  score: number | null;
  ageTicks: number | null;
  misses: number | null;
  latestTrackEvent: TrackReplayEvent | null;
  latestStabilityEvent: TrackStabilityReplayEvent | null;
  trail: Array<{ x: number; y: number; z: number | null; t_s: number }>;
};

export type ReplayFrameInterceptor = {
  interceptorId: number;
  engaged: boolean;
  trackId: number | null;
  x: number | null;
  y: number | null;
  z: number | null;
  velocity: number[];
  engagementTimeS: number | null;
  latestState: InterceptorStateReplayEvent;
  trail: Array<{ x: number; y: number; z: number | null; t_s: number }>;
};

export type ReplayFrameAssignment = {
  interceptorId: number;
  trackId: number;
  reason: string | null;
  decisionType: string | null;
  event: AssignmentReplayEvent;
};

export type ReplayFrame = {
  timeS: number;
  tracks: ReplayFrameTrack[];
  interceptors: ReplayFrameInterceptor[];
  assignments: ReplayFrameAssignment[];
  latestBtDecision: BtDecisionReplayEvent | null;
  recentEvents: ReplayEvent[];
  faults: FaultReplayEvent[];
  intercepts: InterceptReplayEvent[];
};

type TrackSeed = {
  latestTrackEvent: TrackReplayEvent | null;
  latestStabilityEvent: TrackStabilityReplayEvent | null;
};

export function getReplayFrameAt(dataset: ReplayDataset, timeS: number): ReplayFrame {
  return {
    timeS,
    tracks: getActiveTracksAt(dataset, timeS),
    interceptors: getActiveInterceptorsAt(dataset, timeS),
    assignments: getActiveAssignmentsAt(dataset, timeS),
    latestBtDecision: getLatestBtDecisionAt(dataset, timeS),
    recentEvents: getRecentEventsAt(dataset, timeS, 8),
    faults: getEventsBeforeOrAt<FaultReplayEvent>(dataset.faults, timeS),
    intercepts: getEventsBeforeOrAt<InterceptReplayEvent>(
      dataset.events.filter((event): event is InterceptReplayEvent => event.type === "intercept_event"),
      timeS
    )
  };
}

export function getActiveTracksAt(dataset: ReplayDataset, timeS: number): ReplayFrameTrack[] {
  const tracksById = new Map<number, TrackSeed>();

  dataset.tracks.forEach((event) => {
    if (!event.track_id || !isAtOrBefore(event, timeS)) {
      return;
    }
    const seed = tracksById.get(event.track_id) ?? {
      latestTrackEvent: null,
      latestStabilityEvent: null
    };
    if (!seed.latestTrackEvent || compareReplayEvents(event, seed.latestTrackEvent) > 0) {
      seed.latestTrackEvent = event;
    }
    tracksById.set(event.track_id, seed);
  });

  dataset.trackStabilityEvents.forEach((event) => {
    if (!event.track_id || !isAtOrBefore(event, timeS)) {
      return;
    }
    const seed = tracksById.get(event.track_id) ?? {
      latestTrackEvent: null,
      latestStabilityEvent: null
    };
    if (
      !seed.latestStabilityEvent ||
      compareReplayEvents(event, seed.latestStabilityEvent) > 0
    ) {
      seed.latestStabilityEvent = event;
    }
    tracksById.set(event.track_id, seed);
  });

  return [...tracksById.entries()]
    .map(([trackId, seed]) => toFrameTrack(dataset, trackId, seed, timeS))
    .filter((track) => track.status?.toLowerCase() !== "deleted")
    .sort((a, b) => a.trackId - b.trackId);
}

export function getActiveInterceptorsAt(
  dataset: ReplayDataset,
  timeS: number
): ReplayFrameInterceptor[] {
  const latestById = new Map<number, InterceptorStateReplayEvent>();
  dataset.interceptors.forEach((event) => {
    if (!event.interceptor_id || !isAtOrBefore(event, timeS)) {
      return;
    }
    const previous = latestById.get(event.interceptor_id);
    if (!previous || compareReplayEvents(event, previous) > 0) {
      latestById.set(event.interceptor_id, event);
    }
  });

  return [...latestById.values()]
    .map((event) => ({
      interceptorId: event.interceptor_id ?? 0,
      engaged: event.engaged,
      trackId: event.track_id && event.track_id !== 0 ? event.track_id : null,
      x: numberAt(event.position, 0),
      y: numberAt(event.position, 1),
      z: numberAt(event.position, 2),
      velocity: event.velocity,
      engagementTimeS: event.engagement_time_s,
      latestState: event,
      trail: getInterceptorTrail(dataset, event.interceptor_id ?? 0, timeS)
    }))
    .sort((a, b) => a.interceptorId - b.interceptorId);
}

export function getActiveAssignmentsAt(
  dataset: ReplayDataset,
  timeS: number
): ReplayFrameAssignment[] {
  const latestByInterceptor = new Map<number, AssignmentReplayEvent>();
  dataset.assignments.forEach((event) => {
    if (!event.interceptor_id || !event.track_id || !isAtOrBefore(event, timeS)) {
      return;
    }
    const previous = latestByInterceptor.get(event.interceptor_id);
    if (!previous || compareReplayEvents(event, previous) > 0) {
      latestByInterceptor.set(event.interceptor_id, event);
    }
  });

  const intercepts = dataset.events.filter(
    (event): event is InterceptReplayEvent => event.type === "intercept_event"
  );

  return [...latestByInterceptor.values()]
    .filter((assignment) => {
      const intercepted = intercepts.some(
        (event) =>
          event.track_id === assignment.track_id &&
          event.t_s !== null &&
          assignment.t_s !== null &&
          event.t_s >= assignment.t_s &&
          event.t_s <= timeS
      );
      return !intercepted;
    })
    .map((event) => ({
      interceptorId: event.interceptor_id ?? 0,
      trackId: event.track_id ?? 0,
      reason: event.reason,
      decisionType: event.decision_type,
      event
    }))
    .sort((a, b) => a.interceptorId - b.interceptorId);
}

export function getLatestBtDecisionAt(
  dataset: ReplayDataset,
  timeS: number
): BtDecisionReplayEvent | null {
  return getEventsBeforeOrAt(dataset.btDecisions, timeS).at(-1) ?? null;
}

export function getRecentEventsAt(
  dataset: ReplayDataset,
  timeS: number,
  limit: number
): ReplayEvent[] {
  const visibleTypes = new Set<ReplayEvent["type"]>([
    "bt_decision",
    "assignment_event",
    "intercept_event",
    "fault_event",
    "track_stability_event"
  ]);
  return dataset.events
    .filter((event) => visibleTypes.has(event.type) && isAtOrBefore(event, timeS))
    .sort((a, b) => compareReplayEvents(b, a))
    .slice(0, limit);
}

function toFrameTrack(
  dataset: ReplayDataset,
  trackId: number,
  seed: TrackSeed,
  timeS: number
): ReplayFrameTrack {
  const source = seed.latestTrackEvent;
  const stability = seed.latestStabilityEvent;
  return {
    trackId,
    status: stability?.status ?? source?.status ?? null,
    x: numberAt(source?.x ?? [], 0),
    y: numberAt(source?.x ?? [], 1),
    z: numberAt(source?.x ?? [], 2),
    confidence: stability?.confidence ?? source?.confidence ?? null,
    score: stability?.score ?? source?.score ?? null,
    ageTicks: stability?.age_ticks ?? source?.age_ticks ?? null,
    misses: stability?.misses ?? source?.misses ?? null,
    latestTrackEvent: source,
    latestStabilityEvent: stability,
    trail: getTrackTrail(dataset, trackId, timeS)
  };
}

function getTrackTrail(
  dataset: ReplayDataset,
  trackId: number,
  timeS: number
): Array<{ x: number; y: number; z: number | null; t_s: number }> {
  return dataset.tracks
    .filter((event) => event.track_id === trackId && isAtOrBefore(event, timeS))
    .map((event) => ({
      x: numberAt(event.x, 0),
      y: numberAt(event.x, 1),
      z: numberAt(event.x, 2),
      t_s: event.t_s ?? 0
    }))
    .filter((point): point is { x: number; y: number; z: number | null; t_s: number } =>
      point.x !== null && point.y !== null
    )
    .slice(-24);
}

function getInterceptorTrail(
  dataset: ReplayDataset,
  interceptorId: number,
  timeS: number
): Array<{ x: number; y: number; z: number | null; t_s: number }> {
  return dataset.interceptors
    .filter((event) => event.interceptor_id === interceptorId && isAtOrBefore(event, timeS))
    .map((event) => ({
      x: numberAt(event.position, 0),
      y: numberAt(event.position, 1),
      z: numberAt(event.position, 2),
      t_s: event.t_s ?? 0
    }))
    .filter((point): point is { x: number; y: number; z: number | null; t_s: number } =>
      point.x !== null && point.y !== null
    )
    .slice(-24);
}

function getEventsBeforeOrAt<T extends ReplayEvent>(events: T[], timeS: number): T[] {
  return events.filter((event) => isAtOrBefore(event, timeS)).sort(compareReplayEvents);
}

function isAtOrBefore(event: ReplayEvent, timeS: number): boolean {
  return event.t_s !== null && event.t_s <= timeS;
}

function compareReplayEvents(a: ReplayEvent, b: ReplayEvent): number {
  const aTime = a.t_s ?? Number.POSITIVE_INFINITY;
  const bTime = b.t_s ?? Number.POSITIVE_INFINITY;
  if (aTime !== bTime) {
    return aTime - bTime;
  }
  return a.lineNumber - b.lineNumber;
}

function numberAt(values: number[], index: number): number | null {
  return typeof values[index] === "number" ? values[index] : null;
}
